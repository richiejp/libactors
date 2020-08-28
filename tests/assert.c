#include "actors.h"

void some_func(void)
{
	int a = 2, b = 2, c = 5;

	actor_assert(2 + 2 == 4, "This shouldn't trigger!");
	actor_assert(a + b == c, "So %d + %d != %d", a, b, c);
}

int main(void)
{
	some_func();

	return 0;
}
