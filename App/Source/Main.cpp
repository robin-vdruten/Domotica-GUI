#include "Core/Application.h"

#include "MainLayer.h"
#include "LoadingLayer.h"

int main()
{
	Core::ApplicationSpecification appSpec;
	appSpec.Name = "cooked";
	appSpec.WindowSpec.Width = 1920;
	appSpec.WindowSpec.Height = 1080;
	appSpec.WindowSpec.CustomTitlebar = true;

	Core::Application application(appSpec);
	application.PushLayer<MainLayer>();
	application.PushLayer<LoadingLayer>();
	application.Run();
}
