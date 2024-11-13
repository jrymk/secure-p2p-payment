#ifndef LOGIN_H
#define LOGIN_H

#include <gtkmm.h>
#include <iostream>
#include <string>
#include <cctype>
#include "configure_server.h"
#include "register.h"
#include <thread>
#include <atomic>
#include <fstream>

using namespace Glib;
using namespace Gtk;

class LoginForm : public Window {
public:
    std::string username;
    std::string clientPort;

    LoginForm() {
        set_title("Secure P2P Micropayment System");
        set_default_size(440, 230);

        add(scrolledWindow);
        scrolledWindow.add(fixed);

        signInTitle.set_text("Sign in");
        Pango::FontDescription headingFont;
        headingFont.set_size(20 * PANGO_SCALE);
        headingFont.set_weight(Pango::Weight::WEIGHT_BOLD);
        signInTitle.override_font(headingFont);
        fixed.add(signInTitle);
        fixed.move(signInTitle, 15, 20);

        configureServerButton.set_label("Configure server");
        configureServerButton.signal_clicked().connect([this]() {
            configureServerButton.get_style_context()->remove_class("error");
            configureServerWindow.set_transient_for(*this);
            configureServerWindow.show_all();
        });
        fixed.add(configureServerButton);
        fixed.move(configureServerButton, 270, 15);

        usernameHeader.set_text("Username");
        fixed.add(usernameHeader);
        fixed.move(usernameHeader, 15, 70);

        usernameEntry.set_placeholder_text("Enter your username");
        usernameEntry.set_width_chars(25);
        usernameEntry.signal_changed().connect([this]() {
            usernameEntry.get_style_context()->remove_class("error");
        });
        usernameEntry.signal_activate().connect([this]() {
            on_logInButton_clicked(); // so you don't have to click the button, how convenient!
        });
        fixed.add(usernameEntry);
        fixed.move(usernameEntry, 15, 100);

        rememberMe.set_label("Remember me");
        fixed.add(rememberMe);
        fixed.move(rememberMe, 15, 140);

        autoPort.set_label("Auto assign port");
        autoPort.set_active(true);
        autoPort.signal_toggled().connect([this]() {
            portEntry.set_sensitive(!autoPort.get_active());
        });
        fixed.add(autoPort);
        fixed.move(autoPort, 280, 70);

        portEntry.set_placeholder_text("Port");
        portEntry.set_width_chars(5);
        portEntry.signal_changed().connect(sigc::mem_fun(*this, &LoginForm::on_portEntry_changed));
        portEntry.set_sensitive(!autoPort.get_active());
        fixed.add(portEntry);
        fixed.move(portEntry, 280, 100);

        logInButton.set_label("Log in");
        logInButton.set_size_request(130, 20);
        logInButton.get_style_context()->add_class("suggested-action");
        logInButton.signal_clicked().connect(sigc::mem_fun(*this, &LoginForm::on_logInButton_clicked));
        fixed.add(logInButton);
        fixed.move(logInButton, 15, 180);

        registerButton.set_label("Register");
        registerButton.signal_clicked().connect(sigc::mem_fun(*this, &LoginForm::on_registerButton_clicked));
        fixed.add(registerButton);
        fixed.move(registerButton, 165, 180);

        show_all();
    }

private:
    void on_show() override {
        Window::on_show();

        std::ifstream configFile("./client.conf");
        if (configFile.is_open()) {
            std::getline(configFile, username);
            std::getline(configFile, clientPort);
            configFile.close();
        }
        if (clientPort == "0") {
            autoPort.set_active(true);
        } else {
            autoPort.set_active(false);
            portEntry.set_text(clientPort);
        }
    
        usernameEntry.set_text(username);
        portEntry.set_text(clientPort);
        autoPort.set_active(true);
        logInButton.set_sensitive(true);
        logInButton.set_label("Log in");

        usernameEntry.grab_focus();
    }

    void on_portEntry_changed() {
        portEntry.get_style_context()->remove_class("error");
        std::string port = portEntry.get_text();
        // Remove non-digit characters
        port.erase(std::remove_if(port.begin(), port.end(), [](char c) { return !std::isdigit(c); }), port.end());
        portEntry.set_text(port);

        if (port.empty()) {
            portEntry.get_style_context()->remove_class("error");
            return;
        }

        try {
            if (clientAction.p2pListenSocket.checkPort(port) == -1) {
                portEntry.get_style_context()->add_class("error");
            } else {
                portEntry.get_style_context()->remove_class("error");
            }
        } catch (const std::exception &) {
            portEntry.get_style_context()->add_class("error");
        }
    }

    void on_logInButton_clicked() {
        on_portEntry_changed();
        if (usernameEntry.get_text().empty()) {
            usernameEntry.get_style_context()->add_class("error");
            MessageDialog dialog(*this, "Username cannot be empty", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.run();
            return;
        }
        if (!autoPort.get_active()) {
            if (portEntry.get_text().empty()) {
                portEntry.get_style_context()->add_class("error");
                MessageDialog dialog(*this, "Port number cannot be empty", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
                dialog.run();
                return;
            }
            if (clientAction.p2pListenSocket.checkPort(portEntry.get_text()) == -1) {
                portEntry.get_style_context()->add_class("error");
                MessageDialog dialog(*this, "Invalid port number", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
                dialog.run();
                return;
            }
        }
        username = usernameEntry.get_text();
        clientPort = autoPort.get_active() ? "0" : portEntry.get_text();
        configureServerWindow.hide();

        logInButton.set_sensitive(false);
        logInButton.set_label("Logging in...");
        signal_timeout().connect_once(sigc::mem_fun(*this, &LoginForm::logIn), 50);
    }

    void logIn() {
        clientAction.connectToServer(configureServerWindow.serverAddress, configureServerWindow.port);
        if (!clientAction.clientSocket.isConnected) {
            MessageDialog dialog(*this, "Failed to connect to server", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.set_secondary_text(clientAction.error_t);
            dialog.run();

            // configureServerButton.get_style_context()->add_class("error");
            logInButton.set_sensitive(true);
            logInButton.set_label("Log in");
            return;
        }

        if (clientAction.logIn(username, clientPort)) {
            // login success
            logInButton.set_sensitive(true);
            logInButton.set_label("Log in");
            
            clientAction.loggedIn = true;

            // save the username and port to ~/.config/p2ppayment/client.conf
            std::ofstream configFile;

            // create the directory if it doesn't exist
            // Glib::file_set_contents(get_user_config_dir() + "/p2ppayment/client.conf", "");
            
            configFile.open("./client.conf");
            configFile << username << "\n" << clientPort;
            configFile.close();

            hide();
            
        } else {
            MessageDialog dialog(*this, "Login failed", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.set_secondary_text(clientAction.error_t);
            dialog.run();
            logInButton.set_sensitive(true);
            logInButton.set_label("Log in");
        }
    }

    void on_registerButton_clicked() {
        registerWindow.serverAddress = configureServerWindow.serverAddress;
        registerWindow.port = configureServerWindow.port;
        registerWindow.set_transient_for(*this);
        registerWindow.show_all();
    }

    Fixed fixed;
    ScrolledWindow scrolledWindow;
    Label signInTitle;
    Button configureServerButton;
    Label usernameHeader;
    Entry usernameEntry;
    CheckButton rememberMe;
    CheckButton autoPort;
    Entry portEntry;
    Button logInButton;
    Button registerButton;

    ConfigureServerWindow configureServerWindow;
    RegisterWindow registerWindow;
};

#endif // LOGIN_H