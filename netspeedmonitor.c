#include <lxpanel/plugin.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct
{
    GtkWidget* label_tx_rx[2];  //rx tx
    char data[100];    //reusable string buffer
    guint timer;        //Timer
    GtkWidget* vbox;

    char* iface;
    char* label_tx_rx_prefix[2];

    //switch to size_t? ...
    guint last_tx_rx[2];

    int rateunit;
    gboolean preferbitpersec;
    int postfirstlaunch;
    int spacing;
    int preferhorizontal;

    config_setting_t* settings;

}netspeedmon;

const char* rateunits[]=
{
    //byte,bit
    "B/s","b/s",
    "KB/s","Kb/s",
    "MB/s","Mb/s",
};
const char* tx_rx_str[] = { "tx", "rx", };
#define ARY_SZ(arr) sizeof(arr) / sizeof(arr[0])
static gboolean update_cmd(netspeedmon* plugin)
{
    //deltas
    gdouble delta_tx_rx_human[2];
    guint tx_rx[2];
    guint delta_tx_rx[2];
    char tx_rx_text[2][64];
    FILE* fp_cmd;
    int emptyprefix = 0;

    memset(tx_rx, 0, sizeof(tx_rx));

    for(int i = 0; i < 2; i++)
    {
        snprintf(plugin->data, sizeof (plugin->data),
                "cat /sys/class/net/%s/statistics/%s_bytes", plugin->iface, tx_rx_str[i]);
        fp_cmd = popen(plugin->data, "r");
        if(!fp_cmd)
        {
            g_fprintf(stderr, "cannot popen %s\n", plugin->data);
            return TRUE;
        }
        while(fgets(plugin->data, sizeof(plugin->data), fp_cmd))
            continue;;
        if(!pclose(fp_cmd))
        {
            sscanf(plugin->data, "%u", &tx_rx[i]);
            if(plugin->last_tx_rx[i] == 0)
                plugin->last_tx_rx[i] = tx_rx[i];
            delta_tx_rx[i] = tx_rx[i] - plugin->last_tx_rx[i];
            plugin->last_tx_rx[i] = tx_rx[i];
            delta_tx_rx_human[i] = delta_tx_rx[i];
            for (int j = 0; j < plugin->rateunit; j++)
            {
                delta_tx_rx_human[i] /= 1024.0;
            }
            if(plugin->preferbitpersec)
                delta_tx_rx_human[i] *= 8.0;

            snprintf(tx_rx_text[i], sizeof(tx_rx_text[i]),
                    "%6.1f", delta_tx_rx_human[i]);
        }else {
            snprintf(tx_rx_text[i], sizeof(tx_rx_text[i]), "...");
        }
        if(plugin->label_tx_rx_prefix[i] == NULL || (plugin->label_tx_rx_prefix[i] && !strlen(plugin->label_tx_rx_prefix[i])))
            emptyprefix = 1;
        snprintf(plugin->data,sizeof(plugin->data),
                "%s%s%s %s",
                    emptyprefix ? "" : plugin->label_tx_rx_prefix[i],
                    emptyprefix ? "" : ": ",
                tx_rx_text[i],
                rateunits[2*plugin->rateunit+plugin->preferbitpersec]);
        gtk_label_set_text((GtkLabel*)plugin->label_tx_rx[i], plugin->data);
    }
    return TRUE;
}

void netspeed_delete(gpointer user_data)
{
    netspeedmon* nu= (netspeedmon*)user_data;
    g_source_remove(nu->timer);
    g_free(nu->label_tx_rx_prefix[0]);
    g_free(nu->label_tx_rx_prefix[1]);
    g_free(nu->iface);
    g_free(nu);
}
GtkWidget* netspeed_new(LXPanel* panel, config_setting_t* settings)
{
    //Alloc struct
    const char* tmp;
    (void)panel;
    const char* tx_rx_prefix_default[] = {"U", "D"};
    netspeedmon* plugin = g_new0(netspeedmon, 1);
    GtkWidget* vbox = NULL,* evbox = NULL;
    GtkOrientation orientation = GTK_ORIENTATION_VERTICAL;

    memset(plugin, 0, sizeof(netspeedmon));
    plugin->settings=settings;
    config_setting_lookup_int(settings,"postfirstlaunch",&plugin->postfirstlaunch);
    if (!config_setting_lookup_string(settings, "iface", &tmp))
        tmp = "wlan0";
    plugin->iface=g_strdup(tmp);
    for(int i = 0; i < 2; i++)
    {
        snprintf(plugin->data, sizeof(plugin->data), "prefix_%s", tx_rx_str[i]);
        if (!config_setting_lookup_string(settings, plugin->data, &tmp)){
            if(plugin->postfirstlaunch) /* was explicitly set to empty in config */
                tmp = "";
            else
                tmp = tx_rx_prefix_default[i];
        }
        plugin->label_tx_rx_prefix[i] = g_strdup(tmp);
    }

    plugin->rateunit=1;
    config_setting_lookup_int(settings,"rateunit",&plugin->rateunit);
    config_setting_lookup_int(settings,"preferbps",&plugin->preferbitpersec);
    config_setting_lookup_int(settings,"preferhorizontal",&plugin->preferhorizontal);
    if(plugin->preferhorizontal)
        plugin->spacing = 5;
    config_setting_lookup_int(settings,"spacing",&plugin->spacing);

    //Update count
    plugin->timer = g_timeout_add_seconds(1,(GSourceFunc)update_cmd, plugin);

    if(plugin->preferhorizontal)
        orientation = GTK_ORIENTATION_HORIZONTAL;

#ifdef HAVE_GTK_BOX
    vbox = gtk_box_new(orientation, plugin->spacing);
#else
    if(orientation == GTK_ORIENTATION_VERTICAL)
        vbox = gtk_vbox_new(TRUE, plugin->spacing);
    else if(orientation == GTK_ORIENTATION_HORIZONTAL)
        vbox = gtk_hbox_new(TRUE, plugin->spacing);
#endif

    //labels
    for(int i = 0; i < 2; i++)
    {
        plugin->label_tx_rx[i] = gtk_label_new("lxnetspeed");
        gtk_box_pack_start(GTK_BOX(vbox), plugin->label_tx_rx[i], TRUE, TRUE, 0);
    }

    evbox = gtk_event_box_new();
    //Set width

    //add vbox
    gtk_container_add(GTK_CONTAINER(evbox), vbox);
    plugin->vbox = vbox;

    /*gtk_widget_set_size_request(evbox, 100, 25);*/
    //Show all
    gtk_widget_show_all(evbox);

    //Bind struct
    lxpanel_plugin_set_data(evbox, plugin, netspeed_delete);

    //done!
    return evbox;
}

gboolean apply_config(gpointer user_data)
{
    GtkWidget *p = user_data;
    netspeedmon* nu = lxpanel_plugin_get_data(p);
    config_group_set_string(nu->settings, "iface", nu->iface);
    config_group_set_string(nu->settings, "prefix_tx", nu->label_tx_rx_prefix[0]);
    config_group_set_string(nu->settings, "prefix_rx", nu->label_tx_rx_prefix[1]);
    memset(nu->last_tx_rx, 0, sizeof(nu->last_tx_rx));

    int ru= nu->rateunit;
    //guard errors
    if (nu->rateunit > (int)(ARY_SZ(rateunits)/2-1) || nu->rateunit<0) {
        //did you mean KB/s?
        ru=1;
        nu->rateunit=ru;
    }
    config_group_set_int(nu->settings, "rateunit", ru);
    config_group_set_int(nu->settings, "preferbps", nu->preferbitpersec);
    config_group_set_int(nu->settings, "spacing", abs(nu->spacing));
    gtk_box_set_spacing(GTK_BOX(nu->vbox), nu->spacing);
    config_group_set_int(nu->settings, "preferhorizontal", nu->preferhorizontal);
    config_group_set_int(nu->settings, "postfirstlaunch", 1);
    return FALSE;
}

GtkWidget* netspeed_config(LXPanel* panel, GtkWidget* p)
{
    GtkWidget* dlg;
    netspeedmon* nu=lxpanel_plugin_get_data(p);
    dlg=lxpanel_generic_config_dlg("NetSpeedMonitor Configuration",
                                   panel,apply_config,p,
                                   "Interface",&nu->iface,CONF_TYPE_STR,
                                   "Rate unit (None=0, K=1, M=2)",&nu->rateunit,CONF_TYPE_INT,
                                   "Prefer bits per sec",&nu->preferbitpersec,CONF_TYPE_BOOL,
                                   "Transmit Prefix",&nu->label_tx_rx_prefix[0],CONF_TYPE_STR,
                                   "Receive Prefix",&nu->label_tx_rx_prefix[1],CONF_TYPE_STR,
                                   "Spacing",&nu->spacing,CONF_TYPE_INT,
                                   "Prefer horizontal orientation (requires panel restart)",&nu->preferhorizontal,CONF_TYPE_BOOL,
                                   NULL
                                   );
    return dlg;
}

FM_DEFINE_MODULE(lxpanel_gtk, netspeedmonitor);

//Descriptor
LXPanelPluginInit fm_module_init_lxpanel_gtk =
{
    .name = "NetSpeedMonitor",
    .description = "Show network speed",
    .new_instance = netspeed_new,
    .config = netspeed_config,
};
