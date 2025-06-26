#pragma once
#include "Engine/Application/Application.h"

class App : public Okay::Application
{
public:
	App();
	virtual ~App() = default;

protected:
	// Inherited via Application
	void onUpdate(Okay::TimeStep dt) override;

private:
	void updateCamera(Okay::TimeStep dt);

};