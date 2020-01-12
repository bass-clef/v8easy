#pragma once

#include <any>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include <libplatform/libplatform.h>
#include <v8.h>

#pragma comment(lib, "v8.dll.lib")
#pragma comment(lib, "v8_libbase.dll.lib")
#pragma comment(lib, "v8_libplatform.dll.lib")

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
		v8::Isolate::Scope isolate_scope;
		v8::HandleScope handle_scope;
	public:
		scopes(v8::Isolate* isolate) : isolate_scope(isolate), handle_scope(isolate) {}
	};

	std::unique_ptr<v8::Platform> platform;
	v8::Isolate::CreateParams create_params;

	v8::Isolate* global_isolate;
	v8::Local<v8::ObjectTemplate> global_object;
	scopes* global_scopes;

public:
	v8easy(char* argv[]);
	~v8easy();

	// isolateが外部からほしい場合
	operator v8::Isolate*() const { return global_isolate; }

	// v8文字列 の実行
	bool execute(v8::Isolate* isolate, v8::Local<v8::String> source, v8::Local<v8::Value> name,
		std::string& printBuffer, bool doException = false);

	// ファイルから v8文字列 の取得
	v8::MaybeLocal<v8::String> read(v8::Isolate* isolate, const std::string& name);

	// ソース の実行
	const std::string run(const std::string& source, const std::string& fileName = "");

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
