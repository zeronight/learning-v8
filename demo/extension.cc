#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/base/logging.h"

namespace {

using namespace v8;

const char* name = "v8/SimpleObject";
const char* source = R"init_code(
  var simpleObject = {
    add(a, b) {
      native function add();
      return add(a, b);
    },
    print(msg) {
      native function print();
      return print(msg);
    }
  };
)init_code";

class SimpleExtension : public Extension {
 public:
  SimpleExtension(const char* name, const char* source)
      : Extension(name, source) {}

  Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      Isolate* isolate, Local<String> name) override {
    if (strcmp(*String::Utf8Value(isolate, name), "add") == 0) {
      return FunctionTemplate::New(isolate, SimpleExtension::Add);
    } else {
      return FunctionTemplate::New(isolate, SimpleExtension::Print);
    }
  }

  static void Add(const FunctionCallbackInfo<Value>& info) {
    DCHECK(info.Length() == 2);
    DCHECK(info[0]->IsNumber() && info[1]->IsNumber());

    Isolate* isolate = info.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    const double v1 = info[0]->ToNumber(context).ToLocalChecked()->Value();
    const double v2 = info[1]->ToNumber(context).ToLocalChecked()->Value();
    info.GetReturnValue().Set(v1 + v2);
  }

  static void Print(const FunctionCallbackInfo<Value>& info) {
    DCHECK(info.Length() > 0);

    Isolate* isolate = info.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    MaybeLocal<String> message =  info[0]->ToString(context);
    if (!message.IsEmpty()) {
      String::Utf8Value utf8(isolate, message.ToLocalChecked());
      printf("%s\n", *utf8);
    }
  }
};
} // namespace

int main(int argc, char* argv[]) {
  // Initialize V8.
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  // Register extension
  v8::RegisterExtension(std::make_unique<SimpleExtension>(name, source));

  // Create a new Isolate and make it the current one.
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    v8::Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    v8::HandleScope handle_scope(isolate);

    // Create a new context.
    const char* extension_names[] = { name };
    v8::ExtensionConfiguration extensions(1, extension_names);
    v8::Local<v8::Context> context = v8::Context::New(isolate, &extensions);

    // Enter the context for compiling and running the hello world script.
    v8::Context::Scope context_scope(context);

    // Create a string containing the JavaScript source code.
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8Literal(isolate, "simpleObject.print(simpleObject.add(1, 2))");

    // Compile the source code.
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, source).ToLocalChecked();

    // Run the script to get the result.
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

    DCHECK(result->IsUndefined());
  }

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete create_params.array_buffer_allocator;
  return 0;
}
