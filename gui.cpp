#include <iostream>

#include <giomm/notification.h>
#include <giomm/socket.h>
#include <giomm/socketsource.h>
#include <glibmm/main.h>
#include <glibmm/convert.h>
#include <gtkmm/application.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/hvpaned.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <sigc++/functors/mem_fun.h>

#include "lib/commune.h"

#define APP_ID "au.id.tedp.commune.gtkmm"

namespace {

// XXX: Shouldn't inherit from Commune but it's easier for now.
class CommuneGui: public Gtk::Application, public commune::Commune {
public:
    CommuneGui(int argc, char *argv[]);
    int run();

protected:
    void on_startup() override;
    void receiveMessage(const std::string& sender, const std::string& msg) override;
    bool handleSocketEvent(Glib::IOCondition io_condition);
    void processInput();

private:
    Gtk::ApplicationWindow window_;
    Glib::RefPtr<Gtk::TextBuffer> textbufferp_;

    Gtk::VBox vbox_;
    Gtk::HPaned hpane_;
    Gtk::ListBox userlist_;
    Gtk::ScrolledWindow userlist_scroller_;
    Gtk::Entry entry_;
    Gtk::ScrolledWindow textview_scroller_;
    Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark_;
    Gtk::TextView textview_;
};

CommuneGui::CommuneGui(int argc, char *argv[]):
    Gtk::Application(argc, argv, APP_ID),
    commune::Commune::Commune(),
    textbufferp_(Gtk::TextBuffer::create())
{
    window_.set_title("Commune");
    window_.set_default_size(640, 480);

    hpane_.set_position(500);

    textview_.set_buffer(textbufferp_);
    textview_.set_editable(false);
    textview_.set_cursor_visible(false);
    textview_scroller_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
    // TODO: on resize, scroll to end.
    //textview_scroller_.signal_configure_event().connect(/*...*/);
    textview_scroller_.add(textview_);
    end_mark_ = textbufferp_->create_mark("end", textbufferp_->end());
    hpane_.pack1(textview_scroller_, true, true);
    hpane_.pack2(userlist_, true, true);
    vbox_.pack_start(hpane_, true, true);
    vbox_.pack_end(entry_, false, false);

    window_.add(vbox_);

    entry_.grab_focus();
    entry_.signal_activate().connect(sigc::slot<void>(sigc::mem_fun(this, &CommuneGui::processInput)));

    window_.show_all();
}

int CommuneGui::run() {
    return Gtk::Application::run(window_);
}

void CommuneGui::on_startup() {
    Gtk::Application::on_startup();

    Gio::signal_socket().connect(sigc::slot<bool, Glib::IOCondition>(sigc::mem_fun(this, &CommuneGui::handleSocketEvent)), Gio::Socket::create_from_fd(event_fd()), Glib::IO_IN);
}

bool CommuneGui::handleSocketEvent(Glib::IOCondition io_condition) {
    commune::Commune::onEvent();
    return true;
}

void CommuneGui::receiveMessage(const std::string& sender, const std::string& msg) {
    std::string display_message = sender + ": " + msg + '\n';
    auto pos = textbufferp_->insert(textbufferp_->end(), display_message);
    textbufferp_->move_mark(end_mark_, textbufferp_->end());
    textview_.scroll_to(end_mark_);

    auto notification_ref = Gio::Notification::create("Commune message");
    notification_ref->set_body(display_message);
    send_notification("new-message", notification_ref);
}

void CommuneGui::processInput() {
    if (entry_.get_text_length() == 0)
        return;

    std::string msg = Glib::locale_to_utf8(entry_.get_text());

    if (msg.compare(0, 6, "/nick ") == 0) {
        // Maybe truncate at first non-printable (eg. \n)
        // Better do that for incoming nicknames.
        setNick(msg.substr(6));
    } else {
        Commune::sendMessage(msg);
    }

    entry_.set_text(Glib::ustring());
}

} // anonymous namespace

int main(int argc, char *argv[]) {
    return CommuneGui(argc, argv).run();
}
