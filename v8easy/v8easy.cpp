
#include "v8easy.h"

/*
 * ������
 */
v8easy::v8easy(char* argv[]) {
	// �����̏��������O���� isolate �̍쐬�̂Ƃ���ŗ�O���������ē{����
	v8::V8::InitializeICUDefaultLocation(argv[0]);
	v8::V8::InitializeExternalStartupData(argv[0]);
	platform = v8::platform::NewDefaultPlatform();
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();

	// isolate �Ƃ������z���݂����Ȃ��̂����Ȃ��� js �����s�ł��Ȃ��炵���Aisolate �̓}���`�p�ɕ����쐬�ł���
	create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	global_isolate = v8::Isolate::New(create_params);

	// �X�R�[�v�̃X�^�b�N�����Ƃ��Ȃ�Ƃ�(����͏I���܂ŕێ����ĂȂ��Ƒʖ�)
	global_scopes = new scopes(global_isolate);

	// ���� global_object (__main__�݂����Ȃ���)�ɐF�X�Ȋ֐���\��t���Ă���
	global_object = v8::ObjectTemplate::New(global_isolate);
}

/*
 * �I������
 */
v8easy::~v8easy() {
	delete global_scopes;
	global_isolate->Dispose();
	v8::V8::Dispose();
	v8::V8::ShutdownPlatform();
	delete create_params.array_buffer_allocator;
}

/*
 * source ���R���p�C������ ���s
 */
bool v8easy::execute(v8::Isolate* isolate, v8::Local<v8::String> source, v8::Local<v8::Value> name,
	std::string& printBuffer, bool doException) {
	v8::HandleScope handleScope(isolate);
	v8::TryCatch tryCatch(isolate);
	v8::ScriptOrigin origin(name);
	v8::Local<v8::Context> context(isolate->GetCurrentContext());
	v8::Local<v8::Script> script;

	if (!v8::Script::Compile(context, source, &origin).ToLocal(&script)) {
		if (doException) {
			// �R���p�C�����̗�O�������œf�����菈��������
		}
		return false;
	}

	v8::Local<v8::Value> result;
	if (!script->Run(context).ToLocal(&result)) {
		if (doException) {
			// ���s���̗�O�������œf�����菈��������
		}
		return false;
	}

	if (!result->IsUndefined()) {
		printBuffer = from_v8<std::string>(global_isolate, result);
	}

	return true;
}

/*
 * �t�@�C������ǂݏo��
 */
v8::MaybeLocal<v8::String> v8easy::read(v8::Isolate* isolate, const std::string& name) {
	v8::Local<v8::String> v8_source;
	std::ifstream ifs(name);
	if (ifs.fail()) return v8::MaybeLocal<v8::String>();

	std::string source;
	ifs >> source;

	return as<v8::String>(
		to_v8( global_isolate, source, static_cast<int>(std::filesystem::file_size(name)) )
	);
}

/*
 * source �� file �����s���Č��ʂ�Ԃ�
 *
 * �\���I�ɃG���[������ undefined ��Ԃ�
 */
const std::string v8easy::run(const std::string& source, const std::string& fileName) {
	v8::Local<v8::Context> context = v8::Context::New(global_isolate, nullptr, global_object);
	if (context.IsEmpty()) return "undefined";
	v8::Context::Scope scope(context);

	v8::Local<v8::String> v8_source;
	v8::Local<v8::String> v8_fileName = as<v8::String>( to_v8(global_isolate, fileName) );
	if (!read(global_isolate, fileName).ToLocal(&v8_source)) {
		v8_source = as<v8::String>( to_v8(global_isolate, source) );
		v8_fileName = as<v8::String>( to_v8(global_isolate, "unnamed") );
	}

	std::string result;
	bool success = execute(global_isolate, v8_source, v8_fileName, result);
	while (v8::platform::PumpMessageLoop(platform.get(), global_isolate));

	if (!success) {
		return "undefined";
	}
	return std::move(result);
}
