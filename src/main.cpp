#include <iostream>
#include <fstream>
#include <string>

#include <spawner.h>
#include "arguments.h"





int main(int argc, char *argv[]) {
    argument_parser_c parser;
    parser.parse(argc, argv);
    return 0;
    spawner_c spawner(argc, argv);

    spawner.init();
    spawner.run();
    
	return 0;
}
