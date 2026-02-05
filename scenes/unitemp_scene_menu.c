#include "../unitemp.h"
#include "scenes/unitemp_scene.h"

enum SubmenuIndex {
    SubmenuIndexAddNewSensor,
    SubmenuIndexSettings,
    SubmenuIndexHelp,
    SubmenuIndexAbout,
};

void unitemp_scene_menu_on_enter(void* context) {
    UNITEMP_DEBUG("Entering Menu Scene");
    UnitempApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu, "Add new sensor", SubmenuIndexAddNewSensor, unitemp_submenu_callback, app);
    submenu_add_item(submenu, "Settings", SubmenuIndexSettings, unitemp_submenu_callback, app);
    submenu_add_item(submenu, "Help", SubmenuIndexHelp, unitemp_submenu_callback, app);
    submenu_add_item(submenu, "About", SubmenuIndexAbout, unitemp_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, UnitempSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewSubmenu);
}

bool unitemp_scene_menu_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, UnitempSceneMenu, event.event);
        consumed = true;
        if(event.event == SubmenuIndexAddNewSensor) {
            // scene_manager_next_scene(app->scene_manager, UnitempSceneAddNewSensor);
        } else if(event.event == SubmenuIndexSettings) {
            // scene_manager_next_scene(app->scene_manager, UnitempSceneSettings);
        } else if(event.event == SubmenuIndexHelp) {
            // scene_manager_next_scene(app->scene_manager, UnitempSceneHelp);
        } else if(event.event == SubmenuIndexAbout) {
            // scene_manager_next_scene(app->scene_manager, UnitempSceneAbout);
        }
    }

    return consumed;
}

void unitemp_scene_menu_on_exit(void* context) {
    UNITEMP_DEBUG("Exiting Menu Scene");
    UnitempApp* app = context;
    submenu_reset(app->submenu);
}
