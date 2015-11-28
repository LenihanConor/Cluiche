#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <Awesomium/WebCore.h>
#include <Awesomium/BitmapSurface.h>
#include <Awesomium/STLHelpers.h>

#include "DiaUIAwesomium/External/application.h"
#include "DiaUIAwesomium/External/view.h"
#include "DiaUIAwesomium/External/method_dispatcher.h"
#include <Awesomium/WebCore.h>
#include <Awesomium/STLHelpers.h>
#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Time/TimeServer.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <thread>
#include <future>
#include <iostream>

#include <DiaCore/Strings/stringutils.h>

sf::Color backColour;
class TutorialApp : public Application::Listener 
{
public:
	Application* mPlatformAbstractedUIApplicaton;
	Awesomium::WebView* view_;
	MethodDispatcher method_dispatcher_;
	HWND windowHandle_;
	bool isFinishedLoad;
	Awesomium::JSValue app_object

public:
	TutorialApp(HWND windowHandle)
		: mPlatformAbstractedUIApplicaton(Application::Create())
		, view_(nullptr)
		, windowHandle_(windowHandle)
	{
		mPlatformAbstractedUIApplicaton->set_listener(this);
		isFinishedLoad = false;
	}

	virtual ~TutorialApp()
	{
		if (mPlatformAbstractedUIApplicaton)
			delete mPlatformAbstractedUIApplicaton;
	}

	void Do()
	{
		mPlatformAbstractedUIApplicaton->UpdateCore();
	}
	void Run() 
	{
		mPlatformAbstractedUIApplicaton->UpdateCore();
	}

	void Load()
	{
		mPlatformAbstractedUIApplicaton->Load();
	}

	// Inherited from Application::Listener
	virtual void OnLoaded() 
	{
		view_ = mPlatformAbstractedUIApplicaton->web_core()->CreateWebView(800, 600);
		view_->SetTransparent(true);

		app_object = view_->CreateGlobalJavascriptObject(Awesomium::WSLit("app"));
		// Bind our method dispatcher to the WebView
		view_->set_js_method_handler(&method_dispatcher_);

		BindMethods(view_);

		Awesomium::WebURL url(Awesomium::WSLit("file:///Z:/GitHub/Cluiche/Cluiche/WebixTest/app.html"));
		view_->LoadURL(url);

		while (view_->IsLoading())
			mPlatformAbstractedUIApplicaton->web_core()->Update();

	//	Sleep(300);
	//	mPlatformAbstractedUIApplicaton->web_core()->Update();

		isFinishedLoad = true;
	}

	// Inherited from Application::Listener
	virtual void OnUpdate() 
	{
	}

	// Inherited from Application::Listener
	virtual void OnShutdown() {
	}

	void BindMethods(Awesomium::WebView* web_view) {
		// Create a global js object named 'app'
		
		if (app_object.IsObject()) {
			// Bind our custom method to it.
			Awesomium::JSObject& app_object_1 = result.ToObject();
			method_dispatcher_.Bind(app_object_1,
				Awesomium::WSLit("backgroundGrey"),
				JSDelegate(this, &TutorialApp::BackgroundGrey));

			method_dispatcher_.Bind(app_object_1,
				Awesomium::WSLit("backgroundWhite"),
				JSDelegate(this, &TutorialApp::BackgroundWhite));

			method_dispatcher_.Bind(app_object_1,
				Awesomium::WSLit("backgroundBluish"),
				JSDelegate(this, &TutorialApp::BackgroundBluish));
		}

		
	}

	// Bound to app.sayHello() in JavaScript
	void BackgroundGrey(Awesomium::WebView* caller,
		const Awesomium::JSArray& args) {
		backColour = sf::Color(211, 211, 211);
	}

	// Bound to app.sayHello() in JavaScript
	void BackgroundWhite(Awesomium::WebView* caller,
		const Awesomium::JSArray& args) {
		backColour = sf::Color(255, 255, 255);
	}

	// Bound to app.sayHello() in JavaScript
	void BackgroundBluish(Awesomium::WebView* caller,
		const Awesomium::JSArray& args) 
	{
	//	backColour = sf::Color(255, 128, 128);
		Awesomium::WebURL url(Awesomium::WSLit("file:///Z:/GitHub/Cluiche/Cluiche/WebixTest/app2.html"));
		view_->LoadURL(url);

		while (view_->IsLoading())
			mPlatformAbstractedUIApplicaton->web_core()->Update();

		Sleep(300);
		mPlatformAbstractedUIApplicaton->web_core()->Update();

	}

	const Awesomium::BitmapSurface* surface()const
	{
		return static_cast<Awesomium::BitmapSurface*>(view_->surface());
	};


	void SetMouseClick(int _x, int _y)
	{
		view_->InjectMouseMove(_x, _y);
		view_->InjectMouseDown(Awesomium::kMouseButton_Left);
		view_->InjectMouseUp(Awesomium::kMouseButton_Left);
	}

	void SetMouseMove(int _x, int _y)
	{
		view_->InjectMouseMove(_x, _y);
	}
	
};

int main(int argc, const char* argv[])
{
	/* This test is a prototype of how the webix systems works ontop of the the SFML/Awesomium test bed*/

	backColour = sf::Color(211, 211, 211);

	////// create the window -> Start
	wchar_t titleBuffer[1024];
	Dia::Core::StringToWString("OpenGL", &titleBuffer[0]);
	std::wstring titleTempWString(&titleBuffer[0]);

	sf::RenderWindow window(sf::VideoMode(800, 600), sf::String(titleTempWString), sf::Style::Default, sf::ContextSettings(32));

	window.setVerticalSyncEnabled(true);
	window.setActive();
	////// create the window -> End

	///// Initialize UI System -> start
	TutorialApp app(window.getSystemHandle());
	app.Load();
	while (!app.isFinishedLoad)
	{
		int x = 0;
		x++;
	}
		
	sf::RenderTexture backBuffer;

	bool b = backBuffer.create(window.getSize().x, window.getSize().y);

	sf::Sprite uiSprite;
	
	if (!sf::Shader::isAvailable())
	{
		int x = 0;
		x++;
	}

	sf::Shader shader;
	if (!shader.loadFromFile("ui.frag", sf::Shader::Fragment))
	{
		int x = 0;
		x++;
	}

	sf::Texture uiOverlayTex;

	bool a = uiOverlayTex.create(window.getSize().x, window.getSize().y);
	uiSprite.setTexture(uiOverlayTex);
	shader.setParameter("uiOverlayTex", uiOverlayTex);
	//// Initialize UI System -> end




	// Rando Drawing stuff
	sf::CircleShape shape(50);
	shape.setPosition(10, 500);
	shape.setFillColor(sf::Color(150, 50, 250));

	// set a 10-pixel wide orange outline
	shape.setOutlineThickness(10);
	shape.setOutlineColor(sf::Color(250, 150, 100));

	// Enable Z-buffer read and write
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	// Disable lighting
	glDisable(GL_LIGHTING);

	// Configure the viewport (the same size as the window)
	glViewport(0, 0, window.getSize().x, window.getSize().y);

	// Setup a perspective projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	GLfloat ratio = static_cast<float>(window.getSize().x) / window.getSize().y;
	glFrustum(-ratio, ratio, -1.f, 1.f, 1.f, 500.f);
	int i = 0;
	// run the main loop
	bool running = true;
	Dia::Core::TimeServer timeServer(60.0f, Dia::Core::TimeAbsolute::Zero());	// With only one time server everything in the main loop will increment at its frequency
	Dia::Core::TimeThreadLimiter threadLimiter(1.0f / timeServer.GetStep().AsFloatInSeconds());

	while (running)
	{
		threadLimiter.Start();

		// handle events
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
			{
				// end the program
				running = false;
			}
			else if (event.type == sf::Event::Resized)
			{
				// adjust the viewport when the window is resized
				glViewport(0, 0, event.size.width, event.size.height);
			}
			else if (event.type == sf::Event::MouseButtonPressed)
			{
				sf::Event::MouseButtonEvent mouseEvent = event.mouseButton;
				app.SetMouseClick(mouseEvent.x, mouseEvent.y);
			}
			else if (event.type == sf::Event::MouseMoved)
			{
				app.SetMouseMove(event.mouseMove.x, event.mouseMove.y);
			}
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
// clear the buffers
		window.clear(backColour);






		backBuffer.pushGLStates();
			backBuffer.draw(shape);
		backBuffer.popGLStates();






		///// Render UI -> start
		app.Run();

		const Awesomium::BitmapSurface* surface = app.surface();

		int w = surface->width();
		int h = surface->height();

		unsigned char *buffer = new unsigned char[w * h * 4];
		surface->CopyTo(buffer, w * 4, 4, false, false);

		uiOverlayTex.update(buffer);

		if (i == 0)
		{
			sf::Image img = backBuffer.getTexture().copyToImage();
			img.saveToFile("BackBuffer.jpg");
			i++;

			surface->SaveToJPEG(Awesomium::WSLit("./AwesomiumTest.jpg"));

		}

		shader.setParameter("backBufferTex", backBuffer.getTexture()); 

		window.pushGLStates();
		window.draw(uiSprite, &shader);
		window.popGLStates();
		///// Render UI -> start

		delete[] buffer;





		// end the current frame (internally swaps the front and back buffers)
		window.display();


		threadLimiter.Stop();
		std::cout << "Main: Wait " << threadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
		threadLimiter.SleepThread();
	}

	
	
	return 0;
}