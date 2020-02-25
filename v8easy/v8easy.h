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
	 * 変換系
	 */
	// v8::Local<v8::{Hoge}> を v8::Local<v8::Value> に変換
	template<class Value>
	static v8::Local<v8::Value> cast(v8::Local<Value> value) {
		return v8::Local<v8::Value>::Cast(value);
	}
	// v8::Local<v8::Value> を v8::Local<v8::{Hoge}> に変換
	template<class Value>
	static v8::Local<Value> as(v8::Local<v8::Value> value) {
		return value.As<Value>();
	}

	// to
	// <Source=double> を <Dest=v8::Integer> にして v8::Local で内包
	template<class Source = double, class Dest = v8::Number>
	static v8::Local<v8::Value> to_v8(v8::Isolate* isolate, const Source& value, int length = -1) {
		return Dest::New(isolate, *const_cast<Source*>(&value) ).As<v8::Value>();
	}
	// std::string を v8::String にして v8::Local で内包する
	static v8::Local<v8::Value> to_v8(v8::Isolate* isolate, const std::string& value, int length = -1) {
		return cast( v8::String::NewFromUtf8(isolate, value.data(), v8::NewStringType::kNormal, length).ToLocalChecked() );
	}
	// char* を v8::String にして v8::Local で内包する
	static v8::Local<v8::Value> to_v8(v8::Isolate* isolate, const char* value, int length = -1) {
		return cast( v8::String::NewFromUtf8(isolate, value, v8::NewStringType::kNormal, length).ToLocalChecked() );
	}
	// {lambda}void(*)(const v8::FunctionCallBackInfo&) を v8::Local で内包する
	template<>
	static v8::Local<v8::Value> to_v8(v8::Isolate* isolate, const v8::FunctionCallback& value, int length) {
		auto func = v8::FunctionTemplate::New(isolate, value);
		return *reinterpret_cast<v8::Local<v8::Value>*>(const_cast<v8::Local<v8::FunctionTemplate>*>(&func));
	}
	// {lambda}void(*)(v8easy::argument&) を v8::FunctionCallback にしてから v8::Local で内包する
	template<>
	static inline v8::Local<v8::Value> to_v8<callback>(v8::Isolate* isolate, const callback& value, int length) {
		return to_v8(isolate, (v8::FunctionCallback)value);
	}
	// bool を v8::Boolean にして v8::Local で内包
	template<>
	static inline v8::Local<v8::Value> to_v8<bool>(v8::Isolate* isolate, const bool& value, int length) {
		return to_v8<bool, v8::Boolean>(isolate, value);
	}
	// long long を v8::BigInt にして v8::Local で内包
	template<>
	static inline v8::Local<v8::Value> to_v8<int64_t>(v8::Isolate* isolate, const int64_t& value, int length) {
		return to_v8<int64_t, v8::BigInt>(isolate, value);
	}

	// from
	// v8::Local<v8::Hoge> を <Dest=double> にする
	template<class Dest = double, class Source = v8::Number>
	static const Dest from_v8(v8::Isolate* dummy, v8::Local<v8::Value> value) {
		return static_cast<Dest>( v8::Local<Source>::Cast( value )->Value() );
	}
	// v8::Local<v8::Hoge> を <Dest=double> にする
	class object : public v8::Object {
	public:
		object(const v8::Object& parent) : v8::Object(parent) {}
		void* get() { return this->GetAlignedPointerFromInternalField(0); }
	};
	// v8::Local<v8::FunctionTemplate> を無理やり v8easy::callback にする
	template<>
	static const callback from_v8<v8easy::callback, v8::Number>(v8::Isolate* isolate, v8::Local<v8::Value> value) {
//		auto ft = v8::Local<v8::FunctionTemplate>::Cast(value);
		auto ft = *reinterpret_cast<v8::Local<v8::FunctionTemplate>*>(const_cast<v8::Local<v8::Value>*>(&value));
		auto f = ft->GetFunction( v8::Isolate::GetCurrent()->GetCurrentContext() ).ToLocalChecked();
		auto o = (*f)->ToObject( v8::Isolate::GetCurrent() );
		object obj(**o);
		return (callback)obj.get();
	}
	// v8::Local<v8::BigInt> を int64_t にする
	template<>
	static const int64_t from_v8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
		return static_cast<int64_t>( v8::Local<v8::BigInt>::Cast(value)->IntegerValue(isolate->GetCurrentContext()).ToChecked() );
	}
	// v8::Local<v8::String> を std::string にする
	template<>
	static const std::string from_v8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
		std::string string(*v8::String::Utf8Value(isolate, value));
		if (0 == string.length()) return "";
		return std::move(string);
	}

	/* ここから namespace 代わり */
	class array;
	/*
	 * v8::Array 内の v8::Object へのアクセスのためのクラス
	 *
	 * v8::Object(*v8::Array)::Get すると何故かオブジェクト自体がコピーされて元の値が参照できないので仕方なしにarray持ってる
	 */
	class element {
	public:
		array* _array;
		const uint32_t index;

		// キーを any(default=uint32_t) で取得
		template<class Dest = uint32_t, class Source = v8::Number>
		Dest key() {
			return (*_array).keys().get<Dest, Source>(index);
		}

		// キーが any(default=uint32_t) で any(default=double) を取得
		template<class Dest = double, class Source = v8::Number, class KeyDest = uint32_t, class KeySource = v8::Number>
		Dest get() {
			return (*_array).get<Dest, Source>( key<KeyDest, KeySource>() );
		}

		// キーが any(default=uint32_t) で any(default=double) を代入
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
	 * v8 の配列{v8::Local<v8::Array>}へのアクセスのためのクラス
	 *
	 * Object と Array の扱いを同等にできるようにするクラス
	 */
	class array : public v8::Local<v8::Array> {
	public:
		array(v8::Local<v8::Value> ary) : v8::Local<v8::Array>( v8::Local<v8::Array>::Cast(ary) ) {}

		uint32_t length() { return (*this)->Length(); }
		// キーを array でラッピングして取得 (これから取得した array のキーは uint32_t で参照できる)
		array keys() {
			return { (*this)->GetPropertyNames( (*this)->GetIsolate()->GetCurrentContext() ).ToLocalChecked() };
		}

		// キーが any(default=uint32_t) の object をラッピングして取得
		template<class KeySource = double, class KeyDest = v8::Number>
		array get_array(KeySource key) {
			return { (*this)->Get( to_v8(v8::Isolate::GetCurrent(), key)) };
		}

		// キーが any(default=uint32_t) で any(default=double) を取得
		template<class Dest = double, class Source = v8::Number, class KeySource = uint32_t, class KeyDest = v8::Number>
		Dest get(KeySource key) {
			return from_v8<Dest, Source>(
				v8::Isolate::GetCurrent(),
				(*this)->Get( to_v8(v8::Isolate::GetCurrent(), key) )
			);
		}

		// キーが any(default=uint32_t) で any(default=int64_t) を代入
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
	 * v8::Local<v8::Value> のらっぱっぱー
	 */
	class value : public v8::Local<v8::Value> {
	public:
		template<class Source = double, class Dest = v8::Number>
		value(Source value) : v8::Local<v8::Value>( to_v8(v8::Isolate::GetCurrent(), value) ) {}

		// v8::Value を Dest へ変換
		template<class Dest = double, class Source = v8::Number>
		Dest get() { return from_v8<Dest, Source>(v8::Isolate::GetCurrent(), *this ); }

		// v8::Local<v8::Value> への変換
		operator v8::Local<v8::Value>() { return *this; }
	};

	/*
	 * v8::Function を呼ぶのにこんなに書かなきゃいけないとかめんどくね？なクラス
	 *
	 * v8::Object をv8easy::function につっこめば call するだけ
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
	 * ずぼらさんには v8::FunctionCallbackInfo が使いづらいのでラッパークラス
	 */
	class argument : public v8::FunctionCallbackInfo<v8::Value> {
	private:
		v8::Local<v8::Value> at(int index) {
			return (*this)[index];
		}
	public:
		argument(v8::FunctionCallbackInfo<v8::Value>& args) : v8::FunctionCallbackInfo<v8::Value>(args) {}

		// isolateが外部からほしい場合
		operator v8::Isolate* () const { return this->GetIsolate(); }

		// 引数get:any(default=double)
		template<class Dest = double, class Source = v8::Number>
		Dest get(int index) {
			return from_v8<Dest, Source>( this->GetIsolate(), at(index) );
		}
		// 引数:get:v8easy:value
		template<>
		function get(int index) {
			return { at(index) };
		}
		// 引数get:v8easy:array
		array get(int index) {
			return { at(index) };
		};

		// 戻り値get:int
		int result() {
			return from_v8<int, v8::Int32>( this->GetIsolate(), this->GetReturnValue().Get() );
		}

		// 戻り値set:any
		template<class Source = double> void result(Source value) {
			this->GetReturnValue().Set( to_v8( this->GetIsolate(), value ) );
		}
	};

private:
	/*
	 * v8::HandleScope が子クラスでしか new 初期化を許可してくれない(deleteが削除される)ので仕方なしにクラス
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
		自分で V8 をビルドすると高速化されていないオプションを選べるけど {v8_use_external_startup_data = false}
		そうでないので natives_blob.bin/snapshot_blob.bin があるディレクトリを指定して呼び出す必要がある
		詳細:https://groups.google.com/forum/m/#!topic/v8-users/N4GGCkuKnfA
		*/
		v8::V8::InitializeICUDefaultLocation(exePath);
		v8::V8::InitializeExternalStartupData(exePath);
		// これらの初期化を外すと isolate の作成のところで例外が発生して怒られる
		platform = v8::platform::NewDefaultPlatform();
		v8::V8::InitializePlatform(platform.get());
		v8::V8::Initialize();

		// isolate という仮想環境みたいなものを作らないと js が実行できないらしい、isolate はマルチ用に複数作成できる
		create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
		global_isolate = v8::Isolate::New(create_params);
		global_isolate->Enter();

		// スコープのスタックを作るとかなんとか(これは終了まで保持してないと駄目)
		global_scopes = new scopes(global_isolate);

		// この global_object (__main__みたいなもの)に色々な関数を貼り付けていく
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

	// isolateが外部からほしい場合
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
		// 実行/コンパイルする際にコンテキストが必要みたい。(これ毎回スコープと同じ感覚で作成するとメモリで圧迫死する)
		// TODO:かつ C++側で global_object に貼り付ける前に作成して適用すると、
		// C++側で作成したオブジェクトが見えなくなる(まだわからん)
		if (context.IsEmpty()) {
			context = v8::Context::New(global_isolate, nullptr, global_object);
		}
	}

	// ファイルから v8文字列 の取得
	v8::MaybeLocal<v8::String> read(const std::string& fileName) {
		v8::Local<v8::String> v8_source;
		std::ifstream ifs(fileName);
		if (ifs.fail()) return v8::MaybeLocal<v8::String>();

		std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

		// LFのみをCRLFにする (CRLFをLFにしてからCRLFにし直す)
		// LFのみだと JavaScript側で Unexpected Error が起きたため
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
				// コンパイル中の例外をここで吐いたり処理したり
			} else {
				// とりあえずエラー内容だけ返却しとく
				printException(tryCatch, printBuffer);
			}
			return v8::Local<v8::Script>();
		}

		return std::move(script);
	}

	// v8文字列 の実行
	bool execute(v8::Local<v8::Script> script, std::string& printBuffer, bool doException = false) {
		if (script.IsEmpty()) return false;
		useContext(global_context);
		v8::Context::Scope context_scope(global_context);
		v8::HandleScope handle_scope(global_context->GetIsolate());

		v8::TryCatch tryCatch(global_isolate);
		v8::Local<v8::Value> result;
		if (!script->Run(global_isolate->GetCurrentContext()).ToLocal(&result)) {
			if (doException) {
				// コンパイル中の例外をここで吐いたり処理したり
			} else {
				// とりあえずエラー内容だけ返却しとく
				printException(tryCatch, printBuffer);
			}
			return false;
		}

		if (!result->IsUndefined()) {
			printBuffer = from_v8<std::string>(global_isolate, result);
		}

		return true;
	}

	// ソース の実行
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

	// 変数/関数 を定義する
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
