#include "window.hpp"
#include <iostream>

/**
 * @brief this is a test documentation
 *
 * @param TestConst
 */
void test(float TestConst) { std::cout << "testing" << std::endl; }
int main(int argc, char *argv[]) {
	 try {
			SDLWindow gameWindow(800, 600, "My SDL Window");
			const std::string name = "tanner";
			// if (!gameWindow.loadMedia("hello_world.bmp")) {
			//   printf("Failed to load media!\n");
			//   return -1;
			// }
			// Main loop or other logic here

			SDL_Event e;
			while (true) {
				 SDL_PollEvent(&e);
				 if (e.type == SDL_QUIT) {
						break;
				 }
			}
	 } catch (const std::exception &e) {
			printf("Error: %s\n", e.what());
			return -1;
	 }
	 return 0;
}
