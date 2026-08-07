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

void JVM::Init(JavaVM* v, jvmtiEnv* t) {
    vm = v;
    jvmti = t;
    Log("[Summer] JVM initialized");
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

static jclass LoadClassImpl(JNIEnv* env, const std::string& slashed) {
    jclass c = env->FindClass(slashed.c_str());
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (c) return c;

    std::string dotted = slashed;
    for (auto& ch : dotted)
        if (ch == '/') ch = '.';

    jclass threadCls = env->FindClass("java/lang/Thread");
    if (!threadCls) return nullptr;
    jmethodID cur =
        env->GetStaticMethodID(threadCls, "currentThread", "()Ljava/lang/Thread;");
    if (!cur) {
        env->ExceptionClear();
        return nullptr;
    }
    jobject th = env->CallStaticObjectMethod(threadCls, cur);
    if (!th) return nullptr;
    jmethodID gcl =
        env->GetMethodID(threadCls, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    if (!gcl) {
        env->ExceptionClear();
        return nullptr;
    }
    jobject loader = env->CallObjectMethod(th, gcl);
    if (!loader) return nullptr;
    jclass clCls = env->FindClass("java/lang/ClassLoader");
    if (!clCls) return nullptr;
    jmethodID loadClass =
        env->GetMethodID(clCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!loadClass) {
        env->ExceptionClear();
        return nullptr;
    }
    jstring name = env->NewStringUTF(dotted.c_str());
    c = (jclass)env->CallObjectMethod(loader, loadClass, name);
    if (env->ExceptionCheck()) env->ExceptionClear();
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
