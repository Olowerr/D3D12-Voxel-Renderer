
#include "App.h"

int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	srand((uint32_t)time(nullptr));

	App voxelWorld;
	voxelWorld.run();

	return 0;
}
