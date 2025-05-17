// Engine.h
#pragma once

#include "robotick/framework/api.h"

#include <memory>

namespace robotick
{
    class Model;

    class ROBOTICK_API Engine
    {
       public:
        Engine();
        ~Engine();

        void load(const Model& model);
        void setup();
        void start();
        void stop();

       private:
        ROBOTICK_DECLARE_PIMPL();
    };
}  // namespace robotick