#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int main() {
	bool running = true;

	while (running) {
		printf("\n# ");
		char line[2000];
		scanf("%[^\n]", line);
		printf(line);
	}
}
