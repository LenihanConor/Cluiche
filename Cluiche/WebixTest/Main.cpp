#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <Awesomium/WebCore.h>
#include <Awesomium/BitmapSurface.h>
#include <Awesomium/STLHelpers.h>

#include "application.h"
#include "view.h"
#include "method_dispatcher.h"
#include <Awesomium/WebCore.h>
#include <Awesomium/STLHelpers.h>
#ifdef _WIN32
#include <Windows.h>
#endif

#include <thread>
#include <future>

sf::Color backColour;
using namespace Awesomium;
class TutorialApp : public Application::Listener 
{
public:
	Application* app_;
	WebView* view_;
	MethodDispatcher method_dispatcher_;
	HWND windowHandle_;

public:
	TutorialApp(HWND windowHandle)
		: app_(Application::Create())
		, view_(nullptr)
		, windowHandle_(windowHandle)
	{
		app_->set_listener(this);
		isFinished = false;
	}

	virtual ~TutorialApp()
	{
		if (app_)
			delete app_;
	}

	void Run() 
	{
		app_->Run();
	}

	void DoSomething()
	{
		app_->Load();
	}

	// Inherited from Application::Listener
	virtual void OnLoaded() 
	{
		view_ = app_->web_core()->CreateWebView(800, 600);
		view_->SetTransparent(true);

		BindMethods(view_);

		WebURL url(WSLit("file:///C:/_git/Dia/Cluiche/WebixTest/app.html"));
		view_->LoadURL(url);

		while (view_->IsLoading())
			app_->web_core()->Update();

		Sleep(300);
		app_->web_core()->Update();

		isFinished = true;
	}

	// Inherited from Application::Listener
	virtual void OnUpdate() 
	{
	}

	// Inherited from Application::Listener
	virtual void OnShutdown() {
	}

	void BindMethods(WebView* web_view) {
		// Create a global js object named 'app'
		JSValue result = web_view->CreateGlobalJavascriptObject(WSLit("app"));
		if (result.IsObject()) {
			// Bind our custom method to it.
			JSObject& app_object = result.ToObject();
			method_dispatcher_.Bind(app_object,
				WSLit("backgroundGrey"),
				JSDelegate(this, &TutorialApp::BackgroundGrey));

			method_dispatcher_.Bind(app_object,
				WSLit("backgroundWhite"),
				JSDelegate(this, &TutorialApp::BackgroundWhite));

			method_dispatcher_.Bind(app_object,
				WSLit("backgroundBluish"),
				JSDelegate(this, &TutorialApp::BackgroundBluish));
		}

		// Bind our method dispatcher to the WebView
		web_view->set_js_method_handler(&method_dispatcher_);
	}

	// Bound to app.sayHello() in JavaScript
	void BackgroundGrey(WebView* caller,
		const JSArray& args) {
		backColour = sf::Color(211, 211, 211);
	}

	// Bound to app.sayHello() in JavaScript
	void BackgroundWhite(WebView* caller,
		const JSArray& args) {
		backColour = sf::Color(255, 255, 255);
	}

	// Bound to app.sayHello() in JavaScript
	void BackgroundBluish(WebView* caller,
		const JSArray& args) {
		backColour = sf::Color(255, 128, 128);
	}

	const BitmapSurface* surface()const
	{
		return static_cast<BitmapSurface*>(view_->surface());
	};


	void SetMouseClick(int _x, int _y)
	{
		view_->InjectMouseMove(_x, _y);
		view_->InjectMouseDown(kMouseButton_Left);
		view_->InjectMouseUp(kMouseButton_Left);
	}

	void SetMouseMove(int _x, int _y)
	{
		view_->InjectMouseMove(_x, _y);
	}
	bool isFinished;
};

int main(int argc, const char* argv[])
{
	/* This test is a prototype of how the webix systems works ontop of the the SFML/Awesomium test bed*/

	backColour = sf::Color(211, 211, 211);

	// create the window
	sf::RenderWindow window(sf::VideoMode(800, 600), "OpenGL", sf::Style::Default, sf::ContextSettings(32));

	window.setVerticalSyncEnabled(true);
	window.setActive();
	
	TutorialApp app(window.getSystemHandle());
	app.DoSomething();
	while (!app.isFinished)
	{
		int x = 0;
		x++;
	}

	
	sf::RenderTexture backBuffer;

	bool b = backBuffer.create(window.getSize().x, window.getSize().y);
	

	sf::Sprite uiSprite;
	sf::Sprite renderSprite;
	
	

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

	sf::Texture uiOverlayTex;

	bool a = uiOverlayTex.create(window.getSize().x, window.getSize().y);
	uiSprite.setTexture(uiOverlayTex);
	shader.setParameter("uiOverlayTex", uiOverlayTex);

	int i = 0;
	// run the main loop
	bool running = true;
	while (running)
	{
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

		app.Run();

		// clear the buffers
		window.clear(backColour);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
		const BitmapSurface* surface = app.surface();

		int w = surface->width();
		int h = surface->height();

		unsigned char *buffer = new unsigned char[w * h * 4];
		surface->CopyTo(buffer, w * 4, 4, false, false);

		uiOverlayTex.update(buffer);

		backBuffer.pushGLStates();
			backBuffer.draw(shape);
		backBuffer.popGLStates();

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

		delete[] buffer;

		// end the current frame (internally swaps the front and back buffers)
		window.display();

		
	}

	
	
	return 0;
}