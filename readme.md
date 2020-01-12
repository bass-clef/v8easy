v8easy
===

## Description
ずぼらさんの為の JavaScript V8 Library

Easy to use [V8 Engine](https://v8.dev/).

## Usage
1. clone or download this repo.
2. source add header.

    `
        #include "v8easy.h"
    `
- run
    - call `run(const std::string&)` at v8easy instance.
- type
    - put in something.
        ```
	    v8easy::value any[] = {
            true,
            123,
            3.14,
            9223372036854775807,
            'c',
            "string"
        };
        ```
    - function `set("name", v8easy::callback)` at v8easy instance.
        - definition C++ function.
        ```
        void func(v8easy::argument& args) {
            args.result("return value");
        }
        js.set("func_name", func);
        ```
        - lambda C++ function.
        ```
        js.set("func_name", [](v8easy::argument& args){
            v8easy::value
                arg1 = args.get<int>(0),
                arg2 = args.get<int>(1);
            args.result( arg1.get<int>() + arg2.get<int>() );
        });
        ```
        - run.
        ```
        js.run("func_name( 123, 456 );");
        ```

- short source
    ```
    #include <iostream>
    #include "v8easy.h"
    int main(int argc, char* argv[]) {
        v8easy js(argv);
        js.set("version", [](v8easy::argument& args) {
        	args.result(v8::V8::GetVersion());
        });
    	std::cout << js.run("version();") << std::endl;
    	std::cout << std::cin.get();
    }
    ```
___
## Licence and Author
- [GPLv3](http://www.gnu.org/licenses/gpl-3.0.txt) 
- [Humi@bass_clef_](https://twitter.com/bass_clef_)
- bassclef.nico@gmail.com  
