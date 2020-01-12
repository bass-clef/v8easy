
#include <chrono>
#include <iostream>
#include <thread>

#include "v8easy.h"

using argument = v8easy::argument;
void v8_version(argument& args) {
	args.result(v8::V8::GetVersion());
}

// Array/Object 列挙のための関数 (C++はなんでラムダで再帰できないの…)
bool enumNest(v8easy::array ary, int indent) {
	if (ary->IsArray()) {
		std::cout << std::string(indent, '\t') << "Array" << std::endl;
	}else if (ary->IsObject()) {
		std::cout << std::string(indent, '\t') << "Object" << std::endl;
	}else return false;

	std::cout << std::string(indent, '\t') << "(" << std::endl;
	for (auto&& key : ary.keys()) {
		std::cout << std::string(indent + 1, '\t') << "[" << key.get<std::string>() << "] : ";

		if ( !enumNest(ary.get_array(key.get<std::string>()), indent + 1) ) {
			// なんの型で配列を参照するかを明示しないと例外
			std::cout << ary.get<std::string>(key.get<std::string>());
		}

		std::cout << std::endl;
	}
	std::cout << std::string(indent, '\t') << ")" << std::endl;
	return true;
}

int main(int argc, char* argv[]) {
	v8easy js(argv);
	// case 出力
	js.set("version", v8_version);
	// case std::string
	js.set("print", [](argument& args) {
		std::cout << args.get<std::string>(0) << std::endl;
	});
	// case double
	js.set("mul", [](argument& args) {
		args.result( args.get<double>(0) * args.get<double>(1) );
	});
	// case long long
	js.set("add_long", [](argument& args) {
		args.result( args.get<int64_t>(0) + args.get<int64_t>(1) );
	});
	// case int
	js.set("plus", [](argument& args) {
		v8easy::value v1 = 123, v2 = 456;
		args.result( v1.get<int>() + v2.get<int>() + args.get<int>(0) + args.get<int>(1) );
	});
	// case bool
	js.set("not", [](argument& args) {
		args.result( !args.get<bool>(0) );
	});
	// case JavaScript 側の関数をもらって C++ で呼ぶ
	js.set("func", [](argument& args) {
		v8easy::value funcArg[2] = { 45.6, 789 };
		auto func = args.get<v8easy::function>(0);
		args.result(func.call<std::string>(2, funcArg));
	});
	// case JavaScript 側の{配列/オブジェクト}をもらって C++ で参照する
	js.set("print_r", [](argument& args) {
		if (args.get(1)->IsArray()) {
			for (auto&& ary_prm : args.get(0)) {
				// ary_prm の型がはっきりしないと例外発生
				// 宛先が無い Object に代入するときは配列のキーを指定しないと例外発生
				// IsHoge で型判定して set もできるけどめんどいからとりあえず std::string でぶちこむ
				args.get(1).set(ary_prm.index, ary_prm.get<std::string>());
			}
			return;
		}

		enumNest(args.get(0), 1);
	});

	v8easy::value any[] = { true, 123, 3.14, 9223372036854775807, 'c', "string", };
	js.set("func_name", [](v8easy::argument& args) {
		v8easy::value
			arg1 = args.get<int>(0),
			arg2 = args.get<int>(1);
		args.result(arg1.get<int>() + arg2.get<int>());
	});
	std::cout << js.run("func_name( 123, 456 );") << std::endl;

	std::cout << "welcome v8 shell ver." << js.run("version();") << std::endl << ">";
	
	std::string source, line;
	while(true) {
		if (std::getline(std::cin, line)) {
			source += line;
		}
		if (std::cin.eof()) {
			if (source.empty()) break;
			std::cout << "result:" << std::endl << js.run(source) << std::endl << ">";

			std::cin.clear();
			source.clear();
		}
	};

	std::cout << "exit." << std::endl;
	std::this_thread::sleep_for( std::chrono::milliseconds(200) );
	
	return 0;
}
