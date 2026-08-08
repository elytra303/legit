#include "jvm.h"

#include <jvmti.h>

#include <cstring>
#include <unordered_map>

#include "config.h"
#include "util/log.h"

namespace summer {

JavaVM* JVM::vm = nullptr;
jvmtiEnv* JVM::jvmti = nullptr;

static std::unordered_map<std::string, jclass> g_classCache;
static jobject g_gameLoader = nullptr;  // global ref to the game class loader
static bool g_loaderSearched = false;

void JVM::Init(JavaVM* v, jvmtiEnv* t) {
    vm = v;
    jvmti = t;
    Log("[Summer] JVM initialized (jvmti=%s)", jvmti ? "yes" : "no");
}

JNIEnv* JVM::Env() {
    if (vm == nullptr) return nullptr;
    JNIEnv* env = nullptr;
    jint r = vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (r == JNI_OK) return env;
    if (r == JNI_EDETACHED) {
        r = vm->AttachCurrentThread((void**)&env, nullptr);
        if (r == JNI_OK) return env;
    }
    return nullptr;
}

std::string JVM::GetClassName(jclass cls) {
    JNIEnv* env = Env();
    if (!env || !cls) return "";
    jclass clsCls = env->FindClass("java/lang/Class");
    if (!clsCls) return "";
    jmethodID getName = env->GetMethodID(clsCls, "getName", "()Ljava/lang/String;");
    if (!getName) {
        env->ExceptionClear();
        return "";
    }
    jstring s = (jstring)env->CallObjectMethod(cls, getName);
    if (env->ExceptionCheck() || !s) {
        env->ExceptionClear();
        return "";
    }
    const char* utf = env->GetStringUTFChars(s, nullptr);
    std::string out = utf ? utf : "";
    env->ReleaseStringUTFChars(s, utf);
    for (auto& ch : out)
        if (ch == '.') ch = '/';
    return out;
}

std::string JVM::ClassSig(jclass cls) {
    std::string n = GetClassName(cls);
    return n.empty() ? "" : "L" + n + ";";
}

static std::string GetExceptionMsg(JNIEnv* env) {
    if (!env) return "";
    jthrowable ex = env->ExceptionOccurred();
    if (!ex) return "";
    env->ExceptionClear();
    jclass exCls = env->GetObjectClass(ex);
    std::string name = JVM::GetClassName(exCls);
    jclass thrCls = env->FindClass("java/lang/Throwable");
    jmethodID gm =
        thrCls ? env->GetMethodID(thrCls, "getMessage", "()Ljava/lang/String;") : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    std::string msg;
    if (gm) {
        jstring s = (jstring)env->CallObjectMethod(ex, gm);
        if (env->ExceptionCheck()) env->ExceptionClear();
        const char* utf = s ? env->GetStringUTFChars(s, nullptr) : nullptr;
        if (utf) {
            msg = utf;
            env->ReleaseStringUTFChars(s, utf);
        }
        if (s) env->DeleteLocalRef(s);
    }
    env->DeleteLocalRef(ex);
    return name + (msg.empty() ? "" : ": " + msg);
}

static jclass LoadWithLoader(JNIEnv* env, jobject loader, const std::string& dotted) {
    if (!env || !loader) return nullptr;
    jclass clCls = env->FindClass("java/lang/ClassLoader");
    if (!clCls) {
        env->ExceptionClear();
        return nullptr;
    }
    jmethodID loadClass =
        env->GetMethodID(clCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!loadClass) {
        env->ExceptionClear();
        return nullptr;
    }
    jstring name = env->NewStringUTF(dotted.c_str());
    if (!name) {
        env->ExceptionClear();
        return nullptr;
    }
    jclass c = (jclass)env->CallObjectMethod(loader, loadClass, name);
    if (env->ExceptionCheck()) {
        LogWarn("[Summer] loadClass('%s') failed: %s", dotted.c_str(),
                GetExceptionMsg(env).c_str());
        return nullptr;
    }
    return c;
}

static jobject GetContextLoader(JNIEnv* env) {
    if (!env) return nullptr;
    jclass threadCls = env->FindClass("java/lang/Thread");
    if (!threadCls) {
        env->ExceptionClear();
        return nullptr;
    }
    jmethodID cur =
        env->GetStaticMethodID(threadCls, "currentThread", "()Ljava/lang/Thread;");
    if (!cur) {
        env->ExceptionClear();
        return nullptr;
    }
    jobject th = env->CallStaticObjectMethod(threadCls, cur);
    if (!th) {
        env->ExceptionClear();
        return nullptr;
    }
    jmethodID gcl = env->GetMethodID(threadCls, "getContextClassLoader",
                                     "()Ljava/lang/ClassLoader;");
    if (!gcl) {
        env->ExceptionClear();
        return nullptr;
    }
    jobject loader = env->CallObjectMethod(th, gcl);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return loader;
}

// Minecraft's classes are loaded by the class loader of the game's main
// thread ("Render thread" / "main" / "Client thread"). Our swap-hook thread
// has no (or a wrong) context class loader, so grab the game's loader once
// via the cheap JVMTI thread list and reuse it for every lookup. Enumerating
// *all* loaded classes per frame (JVMTI GetLoadedClasses) is far too slow for
// the render thread and was freezing the game - never do that here.
jobject JVM::GameLoader(JNIEnv* env) {
    if (!env) return nullptr;
    if (g_gameLoader) return env->NewLocalRef(g_gameLoader);
    if (g_loaderSearched || !jvmti) return nullptr;
    g_loaderSearched = true;

    jint count = 0;
    jthread* threads = nullptr;
    if (jvmti->GetAllThreads(&count, &threads) != JVMTI_ERROR_NONE || count <= 0) {
        if (threads) jvmti->Deallocate((unsigned char*)threads);
        LogWarn("[Summer] GetAllThreads failed");
        return nullptr;
    }

    static const char* kNames[] = {"Render thread", "main", "Client thread",
                                   "Server thread", "Main Thread", "main thread"};
    jthread found = nullptr;
    for (jint i = 0; i < count && !found; ++i) {
        if (!threads[i]) continue;
        jvmtiThreadInfo info;
        memset(&info, 0, sizeof(info));
        if (jvmti->GetThreadInfo(threads[i], &info) != JVMTI_ERROR_NONE) continue;
        const char* name = info.name ? info.name : "";
        for (const char* k : kNames) {
            if (strcmp(name, k) == 0) {
                found = threads[i];
                break;
            }
        }
        if (!found && strstr(name, "Render")) found = threads[i];
        if (found) {
            std::string matched(info.name ? info.name : "");
            Log("[Summer] loader lookup matched thread '%s'", matched.c_str());
        }
        if (info.name) jvmti->Deallocate((unsigned char*)info.name);
    }
    if (threads) jvmti->Deallocate((unsigned char*)threads);

    jobject loader = nullptr;
    if (found) {
        jvmtiThreadInfo info;
        memset(&info, 0, sizeof(info));
        if (jvmti->GetThreadInfo(found, &info) == JVMTI_ERROR_NONE) {
            if (info.context_class_loader) {
                loader = env->NewGlobalRef(info.context_class_loader);
                env->DeleteLocalRef(info.context_class_loader);
            } else {
                LogWarn("[Summer] matched thread has no context class loader");
            }
            if (info.name) jvmti->Deallocate((unsigned char*)info.name);
        }
    }

    if (loader) {
        g_gameLoader = loader;
        Log("[Summer] cached game class loader");
        return env->NewLocalRef(loader);
    }
    LogWarn("[Summer] game class loader not found");
    return nullptr;
}

static jclass LoadClassImpl(JNIEnv* env, const std::string& slashed) {
    jclass c = env->FindClass(slashed.c_str());
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (c) return c;

    std::string dotted = slashed;
    for (auto& ch : dotted)
        if (ch == '/') ch = '.';

    jobject ctx = GetContextLoader(env);
    c = LoadWithLoader(env, ctx, dotted);
    if (ctx) env->DeleteLocalRef(ctx);
    if (c) return c;

    jobject game = JVM::GameLoader(env);
    c = LoadWithLoader(env, game, dotted);
    if (game) env->DeleteLocalRef(game);
    if (c) Log("[Summer] resolved class %s via game loader", slashed.c_str());
    return c;
}

jclass JVM::FindClass(const char* binName) {
    JNIEnv* env = Env();
    if (!env) return nullptr;
    auto it = g_classCache.find(binName);
    if (it != g_classCache.end()) return it->second;
    jclass local = LoadClassImpl(env, binName);
    if (!local) return nullptr;
    jclass global = (jclass)env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    g_classCache[binName] = global;
    return global;
}

jclass JVM::FindFirstClass(std::initializer_list<const char*> names) {
    for (const char* n : names) {
        jclass c = FindClass(n);
        if (c) return c;
    }
    return nullptr;
}

jfieldID JVM::FindField(const char* key, jclass cls, const char* sig,
                        std::initializer_list<const char*> names) {
    JNIEnv* env = Env();
    if (!env || !cls) return nullptr;

    std::string ov = g_config.Get(std::string("map.") + key, "");
    if (!ov.empty()) {
        jfieldID f = env->GetFieldID(cls, ov.c_str(), sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (f) {
            Log("[Summer] map override %s = %s", key, ov.c_str());
            return f;
        }
    }

    for (const char* n : names) {
        jfieldID f = env->GetFieldID(cls, n, sig);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            continue;
        }
        if (f) return f;
    }
    return nullptr;
}

jmethodID JVM::FindMethod(const char* key, jclass cls, const char* sig,
                          bool isStatic,
                          std::initializer_list<const char*> names) {
    JNIEnv* env = Env();
    if (!env || !cls) return nullptr;

    std::string ov = g_config.Get(std::string("map.") + key, "");
    if (!ov.empty()) {
        jmethodID m = isStatic ? env->GetStaticMethodID(cls, ov.c_str(), sig)
                               : env->GetMethodID(cls, ov.c_str(), sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (m) {
            Log("[Summer] map override %s = %s", key, ov.c_str());
            return m;
        }
    }

    for (const char* n : names) {
        jmethodID m = isStatic ? env->GetStaticMethodID(cls, n, sig)
                               : env->GetMethodID(cls, n, sig);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            continue;
        }
        if (m) return m;
    }
    return nullptr;
}

void JVM::DumpClassByName(const char* binName) {
    JNIEnv* env = Env();
    if (!env) return;
    jclass cls = FindClass(binName);
    if (!cls) {
        Log("[Dump] class not found: %s", binName);
        return;
    }
    Log("[Dump] ===== class %s =====", binName);

    jclass clsCls = env->FindClass("java/lang/Class");
    jmethodID getDeclaredFields =
        env->GetMethodID(clsCls, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
    jmethodID getDeclaredMethods =
        env->GetMethodID(clsCls, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");
    jmethodID f_getName = env->GetMethodID(env->FindClass("java/lang/reflect/Field"),
                                           "getName", "()Ljava/lang/String;");
    jmethodID f_getType = env->GetMethodID(env->FindClass("java/lang/reflect/Field"),
                                           "getType", "()Ljava/lang/Class;");
    jmethodID m_getName = env->GetMethodID(env->FindClass("java/lang/reflect/Method"),
                                           "getName", "()Ljava/lang/String;");
    jmethodID m_getParamTypes =
        env->GetMethodID(env->FindClass("java/lang/reflect/Method"),
                         "getParameterTypes", "()[Ljava/lang/Class;");
    jmethodID m_getReturnType = env->GetMethodID(env->FindClass("java/lang/reflect/Method"),
                                                 "getReturnType", "()Ljava/lang/Class;");
    if (env->ExceptionCheck()) env->ExceptionClear();

    auto nameOf = [&](jobject obj) -> std::string {
        if (!obj) return "?";
        jstring s = (jstring)env->CallObjectMethod(obj, f_getName);
        const char* utf = s ? env->GetStringUTFChars(s, nullptr) : nullptr;
        std::string out = utf ? utf : "?";
        if (s && utf) env->ReleaseStringUTFChars(s, utf);
        return out;
    };

    if (getDeclaredFields) {
        jobjectArray arr = (jobjectArray)env->CallObjectMethod(cls, getDeclaredFields);
        jsize n = arr ? env->GetArrayLength(arr) : 0;
        for (jsize i = 0; i < n; ++i) {
            jobject f = env->GetObjectArrayElement(arr, i);
            jclass t = f ? (jclass)env->CallObjectMethod(f, f_getType) : nullptr;
            Log("[Dump]   field %s : %s", nameOf(f).c_str(), GetClassName(t).c_str());
            env->DeleteLocalRef(f);
        }
        if (arr) env->DeleteLocalRef(arr);
    }
    if (getDeclaredMethods) {
        jobjectArray arr = (jobjectArray)env->CallObjectMethod(cls, getDeclaredMethods);
        jsize n = arr ? env->GetArrayLength(arr) : 0;
        for (jsize i = 0; i < n; ++i) {
            jobject m = env->GetObjectArrayElement(arr, i);
            jclass rt = m ? (jclass)env->CallObjectMethod(m, m_getReturnType) : nullptr;
            jobjectArray params = m ? (jobjectArray)env->CallObjectMethod(m, m_getParamTypes) : nullptr;
            jsize pn = params ? env->GetArrayLength(params) : 0;
            std::string sig = "(";
            for (jsize j = 0; j < pn; ++j) {
                jclass p = (jclass)env->GetObjectArrayElement(params, j);
                sig += ClassSig(p);
                env->DeleteLocalRef(p);
            }
            sig += ")" + ClassSig(rt);
            Log("[Dump]   method %s%s", nameOf(m).c_str(), sig.c_str());
            env->DeleteLocalRef(m);
            if (params) env->DeleteLocalRef(params);
        }
        if (arr) env->DeleteLocalRef(arr);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
}

void JVM::DumpKnownClasses() {
    DumpClassByName("net/minecraft/client/Minecraft");
    DumpClassByName("net/minecraft/world/entity/Entity");
    DumpClassByName("net/minecraft/world/entity/LivingEntity");
    DumpClassByName("net/minecraft/world/entity/player/Player");
    DumpClassByName("net/minecraft/client/player/LocalPlayer");
    DumpClassByName("net/minecraft/client/multiplayer/ClientLevel");
    DumpClassByName("net/minecraft/client/Options");
    DumpClassByName("net/minecraft/client/renderer/GameRenderer");
}

}  // namespace summer
