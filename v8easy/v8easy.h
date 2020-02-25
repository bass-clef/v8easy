#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include <libplatform/libplatform.h>
#include <v8.h>

#ifdef _WIN32
	// Windows
	#pragma comment(lib, "v8.dll.lib")
	#pragma comment(lib, "v8_libbase.dll.lib")
	#pragma comment(lib, "v8_libplatform.dll.lib")
	#ifdef _WIN64
		// 64bit
	#else
		// 32bit
	#endif
#else
	// Linux
	#ifdef __x86_64__
		// 64bit
	#else
		// 32bit
	#endif
#endif

class v8easy {
public:
	class argument;
	typedef void (*callback)(v8easy::argument& info);

	/*
	 * �ϊ��n
	 */
	// v8::Local<v8::{Hoge}> �� v8::Local<v8::Value> �ɕϊ�
	template<class Value>
	static v8::Local<v8::Value> cast(v8::Local<Value> value) {
		return v8::Local<v8::Value>::Cast(value);
	}
	// v8::Local<v8::Value> �� v8::Local<v8::{Hoge}> �ɕϊ�
	template<class Value>
	static v8::Local<Value> as(v8::Local<v8::Value> value) {
		return value.As<Value>();
	}

	// to
	// <Source=double> �� <Dest=v8::Integer> �ɂ��� v8::Local �œ���
	template<class Source = double, class Dest = v8::Number>
	static v8::Local<v8::Value> to_v8(v8::Isolate* isolate, const Source& value, int length = -1) {
		return Dest::New(isolate, *const_cast<Source*>(&value) ).As<v8::Value>();
	}
	// std::string �� v8::String �ɂ��� v8::Local �œ����
	static v8::Local<v8::Value> to_v8(v8::Isolate* isolate, const std::string& value, int length = -1) {
		return cast( v8::String::NewFromUtf8(isolate, value.data(), v8::NewStringType::kNormal, length).ToLocalChecked() );
	}
	// char* �� v8::String �ɂ��� v8::Local �œ����
	static v8::Local<v8::Value> to_v8(v8::Isolate* isolate, const char* value, int length = -1) {
		return cast( v8::String::NewFromUtf8(isolate, value, v8::NewStringType::kNormal, length).ToLocalChecked() );
	}
	// {lambda}void(*)(const v8::FunctionCallBackInfo&) �� v8::Local �œ����
	template<>
	static v8::Local<v8::Value> to_v8(v8::Isolate* isolate, const v8::FunctionCallback& value, int length) {
		auto func = v8::FunctionTemplate::New(isolate, value);
		return *reinterpret_cast<v8::Local<v8::Value>*>(const_cast<v8::Local<v8::FunctionTemplate>*>(&func));
	}
	// {lambda}void(*)(v8easy::argument&) �� v8::FunctionCallback �ɂ��Ă��� v8::Local �œ����
	template<>
	static inline v8::Local<v8::Value> to_v8<callback>(v8::Isolate* isolate, const callback& value, int length) {
		return to_v8(isolate, (v8::FunctionCallback)value);
	}
	// bool �� v8::Boolean �ɂ��� v8::Local �œ���
	template<>
	static inline v8::Local<v8::Value> to_v8<bool>(v8::Isolate* isolate, const bool& value, int length) {
		return to_v8<bool, v8::Boolean>(isolate, value);
	}
	// long long �� v8::BigInt �ɂ��� v8::Local �œ���
	template<>
	static inline v8::Local<v8::Value> to_v8<int64_t>(v8::Isolate* isolate, const int64_t& value, int length) {
		return to_v8<int64_t, v8::BigInt>(isolate, value);
	}

	// from
	// v8::Local<v8::Hoge> �� <Dest=double> �ɂ���
	template<class Dest = double, class Source = v8::Number>
	static const Dest from_v8(v8::Isolate* dummy, v8::Local<v8::Value> value) {
		return static_cast<Dest>( v8::Local<Source>::Cast( value )->Value() );
	}
	// v8::Local<v8::Hoge> �� <Dest=double> �ɂ���
	class object : public v8::Object {
	public:
		object(const v8::Object& parent) : v8::Object(parent) {}
		void* get() { return this->GetAlignedPointerFromInternalField(0); }
	};
	// v8::Local<v8::FunctionTemplate> �𖳗���� v8easy::callback �ɂ���
	template<>
	static const callback from_v8<v8easy::callback, v8::Number>(v8::Isolate* isolate, v8::Local<v8::Value> value) {
//		auto ft = v8::Local<v8::FunctionTemplate>::Cast(value);
		auto ft = *reinterpret_cast<v8::Local<v8::FunctionTemplate>*>(const_cast<v8::Local<v8::Value>*>(&value));
		auto f = ft->GetFunction( v8::Isolate::GetCurrent()->GetCurrentContext() ).ToLocalChecked();
		auto o = (*f)->ToObject( v8::Isolate::GetCurrent() );
		object obj(**o);
		return (callback)obj.get();
	}
	// v8::Local<v8::BigInt> �� int64_t �ɂ���
	template<>
	static const int64_t from_v8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
		return static_cast<int64_t>( v8::Local<v8::BigInt>::Cast(value)->IntegerValue(isolate->GetCurrentContext()).ToChecked() );
	}
	// v8::Local<v8::String> �� std::string �ɂ���
	template<>
	static const std::string from_v8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
		std::string string(*v8::String::Utf8Value(isolate, value));
		if (0 == string.length()) return "";
		return std::move(string);
	}

	/* �������� namespace ���� */
	class array;
	/*
	 * v8::Array ���� v8::Object �ւ̃A�N�Z�X�̂��߂̃N���X
	 *
	 * v8::Object(*v8::Array)::Get ����Ɖ��̂��I�u�W�F�N�g���̂��R�s�[����Č��̒l���Q�Ƃł��Ȃ��̂Ŏd���Ȃ���array�����Ă�
	 */
	class element {
	public:
		array* _array;
		const uint32_t index;

		// �L�[�� any(default=uint32_t) �Ŏ擾
		template<class Dest = uint32_t, class Source = v8::Number>
		Dest key() {
			return (*_array).keys().get<Dest, Source>(index);
		}

		// �L�[�� any(default=uint32_t) �� any(default=double) ���擾
		template<class Dest = double, class Source = v8::Number, class KeyDest = uint32_t, class KeySource = v8::Number>
		Dest get() {
			return (*_array).get<Dest, Source>( key<KeyDest, KeySource>() );
		}

		// �L�[�� any(default=uint32_t) �� any(default=double) ����
		template<class Source = double, class Dest = v8::Number, class KeyDest = uint32_t, class KeySource = v8::Number>
		void set(Source value) {
			(*_array).set<Source, Dest>(key<KeyDest, KeySource>(), value);
		}
	};

	class array_iterator {
	public:
		array* ary;
		uint32_t index = 0;
		element operator*() { return (*ary).at(index); }
		array_iterator& operator++() { ++index; return *this; }
		bool operator!=(const array_iterator& it) { return index != it.index; }
	};
	/*
	 * v8 �̔z��{v8::Local<v8::Array>}�ւ̃A�N�Z�X�̂��߂̃N���X
	 *
	 * Object �� Array �̈����𓯓��ɂł���悤�ɂ���N���X
	 */
	class array : public v8::Local<v8::Array> {
	public:
		array(v8::Local<v8::Value> ary) : v8::Local<v8::Array>( v8::Local<v8::Array>::Cast(ary) ) {}

		uint32_t length() { return (*this)->Length(); }
		// �L�[�� array �Ń��b�s���O���Ď擾 (���ꂩ��擾���� array �̃L�[�� uint32_t �ŎQ�Ƃł���)
		array keys() {
			return { (*this)->GetPropertyNames( (*this)->GetIsolate()->GetCurrentContext() ).ToLocalChecked() };
		}

		// �L�[�� any(default=uint32_t) �� object �����b�s���O���Ď擾
		template<class KeySource = double, class KeyDest = v8::Number>
		array get_array(KeySource key) {
			return { (*this)->Get( to_v8(v8::Isolate::GetCurrent(), key)) };
		}

		// �L�[�� any(default=uint32_t) �� any(default=double) ���擾
		template<class Dest = double, class Source = v8::Number, class KeySource = uint32_t, class KeyDest = v8::Number>
		Dest get(KeySource key) {
			return from_v8<Dest, Source>(
				v8::Isolate::GetCurrent(),
				(*this)->Get( to_v8(v8::Isolate::GetCurrent(), key) )
			);
		}

		// �L�[�� any(default=uint32_t) �� any(default=int64_t) ����
		template<class Source = double, class Dest = v8::Number, class KeySource = uint32_t, class KeyDest = v8::Number>
		void set(KeySource key, Source value) {
			(*this)->Set(
				to_v8<KeySource, KeyDest>(v8::Isolate::GetCurrent(), key),
				to_v8(v8::Isolate::GetCurrent(), value)
			);
		}

		const element& at(uint32_t index) { return { this, index }; }
		array_iterator begin() noexcept { return { this, 0 }; }
		array_iterator end() noexcept { return { this, length() }; }
	};

	/*
	 * v8::Local<v8::Value> �̂���ς��ρ[
	 */
	class value : public v8::Local<v8::Value> {
	public:
		template<class Source = double, class Dest = v8::Number>
		value(Source value) : v8::Local<v8::Value>( to_v8(v8::Isolate::GetCurrent(), value) ) {}

		// v8::Value �� Dest �֕ϊ�
		template<class Dest = double, class Source = v8::Number>
		Dest get() { return from_v8<Dest, Source>(v8::Isolate::GetCurrent(), *this ); }

		// v8::Local<v8::Value> �ւ̕ϊ�
		operator v8::Local<v8::Value>() { return *this; }
	};

	/*
	 * v8::Function ���ĂԂ̂ɂ���Ȃɏ����Ȃ��Ⴂ���Ȃ��Ƃ��߂�ǂ��ˁH�ȃN���X
	 *
	 * v8::Object ��v8easy::function �ɂ����߂� call ���邾��
	 */
	class function : public v8::Local<v8::Object> {
	public:
		function(v8::Local<v8::Value> object) : v8::Local<v8::Object>( object->ToObject(v8::Isolate::GetCurrent()) ) {}

		template<class Source = double, class Dest = v8::Number>
		Source call(int argc, v8easy::value* argv) {
			return from_v8<Source, Dest>(
				v8::Isolate::GetCurrent(),
				(*this)->CallAsFunction(
					v8::Isolate::GetCurrent()->GetCurrentContext(),
					v8::Isolate::GetCurrent()->GetCurrentContext()->Global(),
					argc, argv
				).ToLocalChecked()
			);
		}
	};

	/*
	 * ���ڂ炳��ɂ� v8::FunctionCallbackInfo ���g���Â炢�̂Ń��b�p�[�N���X
	 */
	class argument : public v8::FunctionCallbackInfo<v8::Value> {
	private:
		v8::Local<v8::Value> at(int index) {
			return (*this)[index];
		}
	public:
		argument(v8::FunctionCallbackInfo<v8::Value>& args) : v8::FunctionCallbackInfo<v8::Value>(args) {}

		// isolate���O������ق����ꍇ
		operator v8::Isolate* () const { return this->GetIsolate(); }

		// ����get:any(default=double)
		template<class Dest = double, class Source = v8::Number>
		Dest get(int index) {
			return from_v8<Dest, Source>( this->GetIsolate(), at(index) );
		}
		// ����:get:v8easy:value
		template<>
		function get(int index) {
			return { at(index) };
		}
		// ����get:v8easy:array
		array get(int index) {
			return { at(index) };
		};

		// �߂�lget:int
		int result() {
			return from_v8<int, v8::Int32>( this->GetIsolate(), this->GetReturnValue().Get() );
		}

		// �߂�lset:any
		template<class Source = double> void result(Source value) {
			this->GetReturnValue().Set( to_v8( this->GetIsolate(), value ) );
		}
	};

private:
	/*
	 * v8::HandleScope ���q�N���X�ł��� new �������������Ă���Ȃ�(delete���폜�����)�̂Ŏd���Ȃ��ɃN���X
	 */
	class scopes {
	public:
		v8::Isolate::Scope isolate_scope;
		v8::HandleScope handle_scope;
		scopes(v8::Isolate* isolate) : isolate_scope(isolate), handle_scope(isolate) {}
	};

	std::unique_ptr<v8::Platform> platform;
	v8::Isolate::CreateParams create_params;

	v8::Isolate* global_isolate;
	v8::Local<v8::ObjectTemplate> global_object;
	v8::Local<v8::Context> global_context, run_context;
	scopes* global_scopes;

public:
	v8easy(char* exePath) {
		/*
		������ V8 ���r���h����ƍ���������Ă��Ȃ��I�v�V������I�ׂ邯�� {v8_use_external_startup_data = false}
		�����łȂ��̂� natives_blob.bin/snapshot_blob.bin ������f�B���N�g�����w�肵�ČĂяo���K�v������
		�ڍ�:https://groups.google.com/forum/m/#!topic/v8-users/N4GGCkuKnfA
		*/
		v8::V8::InitializeICUDefaultLocation(exePath);
		v8::V8::InitializeExternalStartupData(exePath);
		// �����̏��������O���� isolate �̍쐬�̂Ƃ���ŗ�O���������ē{����
		platform = v8::platform::NewDefaultPlatform();
		v8::V8::InitializePlatform(platform.get());
		v8::V8::Initialize();

		// isolate �Ƃ������z���݂����Ȃ��̂����Ȃ��� js �����s�ł��Ȃ��炵���Aisolate �̓}���`�p�ɕ����쐬�ł���
		create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
		global_isolate = v8::Isolate::New(create_params);
		global_isolate->Enter();

		// �X�R�[�v�̃X�^�b�N�����Ƃ��Ȃ�Ƃ�(����͏I���܂ŕێ����ĂȂ��Ƒʖ�)
		global_scopes = new scopes(global_isolate);

		// ���� global_object (__main__�݂����Ȃ���)�ɐF�X�Ȋ֐���\��t���Ă���
		global_object = v8::ObjectTemplate::New(global_isolate);
	}
	~v8easy() {
		delete global_scopes;
		global_isolate->Exit();
		global_isolate->Dispose();
		v8::V8::Dispose();
		v8::V8::ShutdownPlatform();
		delete create_params.array_buffer_allocator;
	}

	inline void replace_all(std::string& source, const std::string& from, const std::string& to) {
		if (from.empty()) source;
		size_t p = source.find(from);
		while ((p = source.find(from, p)) != std::string::npos) {
			source.replace(p, from.length(), to);
			p += to.length();
		}
	}

	// isolate���O������ق����ꍇ
	operator v8::Isolate*() const { return global_isolate; }

	void printException(v8::TryCatch& tryCatch, std::string& printBuffer) {
		auto message = tryCatch.Message();
		if (message.IsEmpty()) return;
		auto fileName();
		// print {fileName}:line({line}):col({column begin}-{end}): {message}.
		printBuffer
			= from_v8<std::string>(v8::Isolate::GetCurrent(), message->GetScriptOrigin().ResourceName() )
			+":line("+ std::to_string(message->GetLineNumber(v8::Isolate::GetCurrent()->GetCurrentContext()).FromJust() ) +")"
			+":col("+ std::to_string(message->GetStartColumn()) +"-"+ std::to_string(message->GetEndColumn()) +")"
			+": "+ from_v8<std::string>(v8::Isolate::GetCurrent(), as<v8::String>(tryCatch.Exception()) );
	}

	void useContext(v8::Local<v8::Context>& context) {
		// ���s/�R���p�C������ۂɃR���e�L�X�g���K�v�݂����B(���ꖈ��X�R�[�v�Ɠ������o�ō쐬����ƃ������ň���������)
		// TODO:���� C++���� global_object �ɓ\��t����O�ɍ쐬���ēK�p����ƁA
		// C++���ō쐬�����I�u�W�F�N�g�������Ȃ��Ȃ�(�܂��킩���)
		if (context.IsEmpty()) {
			context = v8::Context::New(global_isolate, nullptr, global_object);
		}
	}

	// �t�@�C������ v8������ �̎擾
	v8::MaybeLocal<v8::String> read(const std::string& fileName) {
		v8::Local<v8::String> v8_source;
		std::ifstream ifs(fileName);
		if (ifs.fail()) return v8::MaybeLocal<v8::String>();

		std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

		// LF�݂̂�CRLF�ɂ��� (CRLF��LF�ɂ��Ă���CRLF�ɂ�����)
		// LF�݂̂��� JavaScript���� Unexpected Error ���N��������
		replace_all(source, { 0xD, 0xA }, { 0xA });
		replace_all(source, { 0xA }, { 0xD, 0xA });

		return as<v8::String>(
			to_v8(global_isolate, source, static_cast<int>(std::filesystem::file_size(fileName)))
			);
	}

	v8::Local<v8::Script> compile(std::string& printBuffer, const std::string& source, const std::string& fileName = "", bool doException = false) {
		useContext(global_context);
		v8::Local<v8::String> v8_source;
		v8::Local<v8::String> v8_fileName = as<v8::String>(to_v8(global_isolate, fileName));
		if (!read(fileName).ToLocal(&v8_source)) {
			v8_source = as<v8::String>(to_v8(global_isolate, source));
			v8_fileName = as<v8::String>(to_v8(global_isolate, "unnamed"));
		}

		v8::TryCatch tryCatch(global_isolate);
		v8::ScriptOrigin origin(v8_fileName);
		v8::Local<v8::Script> script;

		if (!v8::Script::Compile(global_isolate->GetCurrentContext(), v8_source, &origin).ToLocal(&script)) {
			if (doException) {
				// �R���p�C�����̗�O�������œf�����菈��������
			} else {
				// �Ƃ肠�����G���[���e�����ԋp���Ƃ�
				printException(tryCatch, printBuffer);
			}
			return v8::Local<v8::Script>();
		}

		return std::move(script);
	}

	// v8������ �̎��s
	bool execute(v8::Local<v8::Script> script, std::string& printBuffer, bool doException = false) {
		if (script.IsEmpty()) return false;
		useContext(global_context);
		v8::Context::Scope context_scope(global_context);
		v8::HandleScope handle_scope(global_context->GetIsolate());

		v8::TryCatch tryCatch(global_isolate);
		v8::Local<v8::Value> result;
		if (!script->Run(global_isolate->GetCurrentContext()).ToLocal(&result)) {
			if (doException) {
				// �R���p�C�����̗�O�������œf�����菈��������
			} else {
				// �Ƃ肠�����G���[���e�����ԋp���Ƃ�
				printException(tryCatch, printBuffer);
			}
			return false;
		}

		if (!result->IsUndefined()) {
			printBuffer = from_v8<std::string>(global_isolate, result);
		}

		return true;
	}

	// �\�[�X �̎��s
	const std::string run(const std::string& source, const std::string& fileName = "") {
		useContext(global_context);
		v8::Context::Scope context_scope(global_context);

		std::string result;
		v8::Local<v8::Script> script = compile(result, source, fileName);
		bool success = execute(script, result);

		while(v8::platform::PumpMessageLoop(platform.get(), global_isolate))
			continue;

		return std::move(result);
	}

	// �ϐ�/�֐� ���`����
	void set(const std::string& key, v8::FunctionCallback info) {
		global_object->Set(
			as<v8::String>(to_v8(global_isolate, key)),
			to_v8(global_isolate, info)
		);
	}
	void set(const std::string& key, v8easy::callback info) {
		this->set(key, (v8::FunctionCallback)info);
	}
};
