/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <OvTests/TestUtils.h>

int main(int argc, char** argv)
{
	if (argc > 0)
		OvTests::SetProgramPath(argv[0]);

	doctest::Context context(argc, argv);
	return context.run();
}
