#pragma once

namespace eng
{
    namespace commands
    {
        class Move
        {
            
        public:
            //for usage
            
            Move parseFromMem();
            bool storeToMem(void* mem);

            //for eng register and subsystem
            bool registerCommand();
        };
    }
}
