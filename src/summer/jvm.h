#pragma once

#include <jni.h>
#include <jvmti.h>

#include <initializer_list>
#include <string>

namespace summer {

// RAII local reference frame.
class JNIScope {
public:
    explicit JNIScope(JNIEnv* e) : env(e) {
        if (env) env->PushLocalFrame(2048);
    }
    ~JNIScope() {
        if (env) env->PopLocalFrame(nullptr);
    }
    JNIEnv* env = nullptr;
};

class JVM {
public:
    static JavaVM* vm;
    static jvmtiEnv* jvmti;

    static void Init(JavaVM* v, jvmtiEnv* t);
    static JNIEnv* Env();  // attaches current thread if needed

    static jclass FindClass(const char* binName);
    static jclass FindFirstClass(std::initializer_list<const char*> names);
    static jobject GameLoader(JNIEnv* env);  // local ref to game class loader
    static std::string GetClassName(jclass cls);  // slashed binary name
    static std::string ClassSig(jclass cls);      // "Lnet/.../Class;"

    // Resolve with mapping override support (config key "map.<key>" wins).
    static jfieldID FindField(const char* key, jclass cls, const char* sig,
                              std::initializer_list<const char*> names);
    static jmethodID FindMethod(const char* key, jclass cls, const char* sig,
                                bool isStatic,
                                std::initializer_list<const char*> names);

    static void DumpClassByName(const char* binName);
    static void DumpKnownClasses();
};

}  // namespace summer
