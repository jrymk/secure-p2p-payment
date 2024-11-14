#ifndef CONFIGURE_SERVER_H
#define CONFIGURE_SERVER_H

#include <gtkmm.h>
#include "../clientAction.h"

using namespace Glib;
using namespace Gtk;

class ConfigureServerWindow : public Window {
public:
    std::string serverAddress = "localhost";
    std::string port = "5050";

    ConfigureServerWindow() {
        set_title("Configure Server");
        set_default_size(300, 100);

        grid.set_row_spacing(10);
        grid.set_column_spacing(10);

        grid.set_margin_top(15);
        grid.set_margin_bottom(15);
        grid.set_margin_start(15);
        grid.set_margin_end(15);

        add(grid);

        serverAddressLabel.set_text("IP/Hostname:");
        grid.attach(serverAddressLabel, 0, 0, 1, 1);

        serverAddressEntry.set_placeholder_text("Server IP/Hostname");
        serverAddressEntry.set_text(serverAddress);
        serverAddressEntry.signal_changed().connect(sigc::mem_fun(*this, &ConfigureServerWindow::on_serverAddressEntry_changed));
        serverAddressEntry.signal_activate().connect([this]() {
            portEntry.grab_focus();
        });
        grid.attach(serverAddressEntry, 1, 0, 1, 1);

        portLabel.set_text("Port:");
        portLabel.set_halign(Align::ALIGN_END);
        grid.attach(portLabel, 0, 1, 1, 1);

        portEntry.set_placeholder_text("Port number");
        portEntry.signal_changed().connect(sigc::mem_fun(*this, &ConfigureServerWindow::on_portEntry_changed));
        portEntry.signal_activate().connect([this]() {
            on_saveButton_clicked();
        });
        portEntry.set_text(port);
        grid.attach(portEntry, 1, 1, 1, 1);

        testConnectionButton.set_label("Test Connection");
        testConnectionButton.signal_clicked().connect(sigc::mem_fun(*this, &ConfigureServerWindow::on_testConnectionButton_clicked));
        grid.attach(testConnectionButton, 0, 2, 2, 1);

        saveButton.set_label("Save");
        saveButton.get_style_context()->add_class("suggested-action");
        saveButton.signal_clicked().connect(sigc::mem_fun(*this, &ConfigureServerWindow::on_saveButton_clicked));
        grid.attach(saveButton, 0, 3, 2, 1);

        show_all_children();
    }

    void on_show() override {
        Window::on_show();
        serverAddressEntry.set_text(serverAddress);
        portEntry.set_text(port);
        testConnectionButton.set_label("Test Connection");
        testConnectionButton.get_style_context()->remove_class("connection_success");
        testConnectionButton.get_style_context()->remove_class("connection_fail");
        serverAddressEntry.grab_focus();
    }

private:
    void on_serverAddressEntry_changed() {
        testConnectionButton.set_label("Test Connection");
        testConnectionButton.get_style_context()->remove_class("connection_success");
        testConnectionButton.get_style_context()->remove_class("connection_fail");

        serverAddress = serverAddressEntry.get_text();
    }

    void on_portEntry_changed() {
        testConnectionButton.set_label("Test Connection");
        testConnectionButton.get_style_context()->remove_class("connection_success");
        testConnectionButton.get_style_context()->remove_class("connection_fail");

        std::string port = portEntry.get_text();
        // Remove non-digit characters
        port.erase(std::remove_if(port.begin(), port.end(), [](char c) { return !std::isdigit(c); }), port.end());
        portEntry.set_text(port);

        if (port.empty()) {
            portEntry.get_style_context()->remove_class("error");
            // popover.hide();
            return;
        }

        try {
            if (clientAction.p2pListenSocket.checkPort(port) == -1) {
                portEntry.get_style_context()->add_class("error");
            } else {
                portEntry.get_style_context()->remove_class("error");
                this->port = portEntry.get_text();
            }
        } catch (const std::exception &) {
            portEntry.get_style_context()->add_class("error");
        }
    }

    void on_testConnectionButton_clicked() {
        // Handle test connection button click
        std::string serverAddress = serverAddressEntry.get_text();
        std::string port = portEntry.get_text();

        testConnectionButton.set_label("Testing connection...");
        testConnectionButton.get_style_context()->remove_class("connection_success");
        testConnectionButton.get_style_context()->remove_class("connection_fail");

        signal_timeout().connect_once(sigc::mem_fun(*this, &ConfigureServerWindow::testConnection), 50);
    }

    void testConnection() {
        if (serverAddress.empty()) {
            testConnectionButton.get_style_context()->add_class("connection_fail");
            testConnectionButton.set_label("Fail: Server IP/Hostname empty");
            return;
        }
        if (port.empty()) {
            testConnectionButton.get_style_context()->add_class("connection_fail");
            testConnectionButton.set_label("Fail: Port empty");
            return;
        }

        // Test the connection
        std::cout << "Testing connection to " << serverAddress << ":" << port << std::endl;
        ClientAction testConnection;
        bool connectionResult = testConnection.connectToServer(serverAddress, port);
        if (connectionResult) {
            testConnectionButton.get_style_context()->add_class("connection_success");
            testConnectionButton.set_label("Connection successful");
        } else {
            testConnectionButton.get_style_context()->add_class("connection_fail");
            testConnectionButton.set_label("Fail: " + testConnection.error_t);
        }
    }

    void on_saveButton_clicked() {
        // Handle save button click
        serverAddress = serverAddressEntry.get_text();
        port = portEntry.get_text();
        // Save the server address and port
        std::cout << "Server Address: " << serverAddress << std::endl;
        std::cout << "Port: " << port << std::endl;
        hide(); // Close the window
    }

    Grid grid;
    Label serverAddressLabel;
    Entry serverAddressEntry;
    Label portLabel;
    Entry portEntry;
    Button saveButton;
    Button testConnectionButton;
};

#endif // CONFIGURE_SERVER_H