#include <iostream>
#include <string>
#include "login.h"
#include "main_window.h"
#include <gtkmm/application.h>

int main(int argc, char *argv[]) {
    auto app = Gtk::Application::create(argc, argv, "uk.jerrymk.p2ppayment");
    
    app->signal_startup().connect([&app]() {
        LoginForm *loginForm = new LoginForm();
        MainWindow *mainWindow = new MainWindow();

        mainWindow->set_transient_for(*loginForm);

        // Set the main window as the default window
        // app->add_window(*loginForm);
        app->hold();

        loginForm->show_all();

        loginForm->signal_hide().connect([&app, mainWindow]() {
            std::cerr << "login window hidden" << std::endl;
            if (clientAction.loggedIn) { // login action
                app->hold();             // hold for the main window
                mainWindow->show_all();
            } else {
                clientAction.quitApp();
                // app->release();
            }
            app->release();
        });

        // Release the application when the main window is closed
        mainWindow->signal_hide().connect([&app, loginForm]() {
            if (clientAction.loggedIn) {
                clientAction.quitApp();
            } else {
                clientAction.quitApp();
                app->hold();
                loginForm->show_all();
            }
            app->release();
        });
    });

    return app->run();
}