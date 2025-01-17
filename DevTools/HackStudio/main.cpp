#include "FindInFilesWidget.h"
#include "Project.h"
#include "TerminalWrapper.h"
#include <LibCore/CFile.h>
#include <LibGUI/GAboutDialog.h>
#include <LibGUI/GAction.h>
#include <LibGUI/GApplication.h>
#include <LibGUI/GBoxLayout.h>
#include <LibGUI/GButton.h>
#include <LibGUI/GInputBox.h>
#include <LibGUI/GListView.h>
#include <LibGUI/GMenu.h>
#include <LibGUI/GMenuBar.h>
#include <LibGUI/GMessageBox.h>
#include <LibGUI/GSplitter.h>
#include <LibGUI/GStatusBar.h>
#include <LibGUI/GTabWidget.h>
#include <LibGUI/GTextBox.h>
#include <LibGUI/GTextEditor.h>
#include <LibGUI/GToolBar.h>
#include <LibGUI/GWidget.h>
#include <LibGUI/GWindow.h>
#include <stdio.h>
#include <unistd.h>

String g_currently_open_file;
OwnPtr<Project> g_project;
RefPtr<GWindow> g_window;
RefPtr<GListView> g_project_list_view;
RefPtr<GTextEditor> g_text_editor;

static void build(TerminalWrapper&);
static void run(TerminalWrapper&);
void open_file(const String&);

int main(int argc, char** argv)
{
    GApplication app(argc, argv);

    g_window = GWindow::construct();
    g_window->set_rect(100, 100, 800, 600);
    g_window->set_title("HackStudio");

    auto widget = GWidget::construct();
    g_window->set_main_widget(widget);

    widget->set_fill_with_background_color(true);
    widget->set_layout(make<GBoxLayout>(Orientation::Vertical));
    widget->layout()->set_spacing(0);

    if (chdir("/home/anon/little") < 0) {
        perror("chdir");
        return 1;
    }
    g_project = Project::load_from_file("little.files");
    ASSERT(g_project);

    auto toolbar = GToolBar::construct(widget);

    auto outer_splitter = GSplitter::construct(Orientation::Horizontal, widget);
    g_project_list_view = GListView::construct(outer_splitter);
    g_project_list_view->set_model(g_project->model());
    g_project_list_view->set_size_policy(SizePolicy::Fixed, SizePolicy::Fill);
    g_project_list_view->set_preferred_size(200, 0);

    auto inner_splitter = GSplitter::construct(Orientation::Vertical, outer_splitter);
    g_text_editor = GTextEditor::construct(GTextEditor::MultiLine, inner_splitter);
    g_text_editor->set_ruler_visible(true);

    g_project_list_view->on_activation = [&](auto& index) {
        auto filename = g_project_list_view->model()->data(index).to_string();
        open_file(filename);
    };

    auto tab_widget = GTabWidget::construct(inner_splitter);

    auto find_in_files_widget = FindInFilesWidget::construct(nullptr);
    tab_widget->add_widget("Find in files", find_in_files_widget);

    auto terminal_wrapper = TerminalWrapper::construct(nullptr);
    tab_widget->add_widget("Console", terminal_wrapper);

    auto statusbar = GStatusBar::construct(widget);

    g_text_editor->on_cursor_change = [&] {
        statusbar->set_text(String::format("Line: %d, Column: %d", g_text_editor->cursor().line(), g_text_editor->cursor().column()));
    };

    g_text_editor->add_custom_context_menu_action(GAction::create(
        "Go to line...", { Mod_Ctrl, Key_L }, GraphicsBitmap::load_from_file("/res/icons/16x16/go-forward.png"), [&](auto&) {
            auto input_box = GInputBox::construct("Line:", "Go to line", g_window);
            auto result = input_box->exec();
            if (result == GInputBox::ExecOK) {
                bool ok;
                auto line_number = input_box->text_value().to_uint(ok);
                if (ok) {
                    g_text_editor->set_cursor(line_number - 1, 0);
                }
            }
        },
        g_text_editor));

    auto menubar = make<GMenuBar>();
    auto app_menu = make<GMenu>("HackStudio");
    app_menu->add_action(GAction::create("Save", { Mod_Ctrl, Key_S }, GraphicsBitmap::load_from_file("/res/icons/16x16/save.png"), [&](auto&) {
        if (g_currently_open_file.is_empty())
            return;
        g_text_editor->write_to_file(g_currently_open_file);
    }));
    app_menu->add_action(GCommonActions::make_quit_action([&](auto&) {
        app.quit();
    }));
    menubar->add_menu(move(app_menu));

    auto edit_menu = make<GMenu>("Edit");
    edit_menu->add_action(GAction::create("Find in files...", { Mod_Ctrl | Mod_Shift, Key_F }, [&](auto&) {
        tab_widget->set_active_widget(find_in_files_widget);
        find_in_files_widget->focus_textbox_and_select_all();
    }));
    menubar->add_menu(move(edit_menu));

    auto build_menu = make<GMenu>("Build");
    build_menu->add_action(GAction::create("Build", { Mod_Ctrl, Key_B }, [&](auto&) {
        build(terminal_wrapper);
    }));
    build_menu->add_action(GAction::create("Run", { Mod_Ctrl, Key_R }, [&](auto&) {
        run(terminal_wrapper);
    }));
    menubar->add_menu(move(build_menu));

    auto small_icon = GraphicsBitmap::load_from_file("/res/icons/16x16/app-hack-studio.png");

    auto help_menu = make<GMenu>("Help");
    help_menu->add_action(GAction::create("About", [&](auto&) {
        GAboutDialog::show("HackStudio", small_icon, g_window);
    }));
    menubar->add_menu(move(help_menu));

    app.set_menubar(move(menubar));

    g_window->set_icon(small_icon);

    g_window->show();
    return app.exec();
}

void build(TerminalWrapper& wrapper)
{
    wrapper.run_command("make");
}

void run(TerminalWrapper& wrapper)
{
    wrapper.run_command("make run");
}

void open_file(const String& filename)
{
    auto file = CFile::construct(filename);
    if (!file->open(CFile::ReadOnly)) {
        GMessageBox::show("Could not open!", "Error", GMessageBox::Type::Error, GMessageBox::InputType::OK, g_window);
        return;
    }
    g_text_editor->set_text(file->read_all());
    g_currently_open_file = filename;
    g_window->set_title(String::format("%s - HackStudio", g_currently_open_file.characters()));
    g_project_list_view->update();
}
