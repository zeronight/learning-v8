#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdio>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/common/assert-scope.h"
#include "src/execution/isolate.h"
#include "v8-internal.h"

static inline v8::Local<v8::String> v8_str(const char* x) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), x).ToLocalChecked();
}

int main(int argc, char* argv[]) {
  // Initialize V8.
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  // set hash seed.
  i::FLAG_hash_seed = 1337;

  const char* source = "Math.sqrt(16)";
  const char* origin = "test origin";

  v8::ScriptCompiler::CachedData* code_cache;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate1);
    v8::HandleScope handle_scope(isolate1);
    v8::Local<v8::Context> context = v8::Context::New(isolate1);
    v8::Context::Scope context_scope(context);
    v8::Local<v8::String> source_string = v8_str(source);
    v8::ScriptOrigin script_origin(isolate1, v8_str(origin));
    v8::ScriptCompiler::Source source(source_string, script_origin);

    // Compile the source code.
    v8::ScriptCompiler::CompileOptions option = v8::ScriptCompiler::kNoCompileOptions;
    v8::Local<v8::Script> script =
        v8::ScriptCompiler::Compile(context, &source, option).ToLocalChecked();

    code_cache = v8::ScriptCompiler::CreateCodeCache(script->GetUnboundScript());
  }
  isolate1->Dispose();

  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope iscope(isolate2);
    v8::HandleScope scope(isolate2);
    v8::Local<v8::Context> context = v8::Context::New(isolate2);
    v8::Context::Scope cscope(context);
    v8::Local<v8::String> source_string = v8_str(source);
    v8::ScriptOrigin script_origin(isolate2, v8_str(origin));

    // Compile the source code.
    v8::ScriptCompiler::Source source(source_string, script_origin, code_cache);
    v8::ScriptCompiler::CompileOptions option = v8::ScriptCompiler::kConsumeCodeCache;
    v8::Local<v8::Script> script;
    {
      i::DisallowCompilation no_compile(reinterpret_cast<i::Isolate*>(isolate2));
      script = v8::ScriptCompiler::Compile(context, &source, option).ToLocalChecked();
    }

    uint x = script->Run(context)
                 .ToLocalChecked()
                 ->ToInt32(context)
                .ToLocalChecked()
                ->Int32Value(context)
                .FromJust();
    printf("%d\n", x);
  }
  isolate2->Dispose();

  // Dispose the isolate and tear down V8.
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete create_params.array_buffer_allocator;
  return 0;
}
