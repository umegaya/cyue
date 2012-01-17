#include "app.h"
#include "server.h"

using namespace yue;

int main(int argc, char *argv[]) {
	util::app a;
	return a.run<server>(argc, argv);
}

