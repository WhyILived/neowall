#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <limits.h>

#include "../../include/neowall.h"
#include "../../include/constellation.h"

#define MAX_JSON_SIZE 4096

static char socket_path[512];

const char *constellation_get_socket_path(void) {
    return socket_path;
}

static void init_socket_path(void) {
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (runtime_dir) {
        snprintf(socket_path, sizeof(socket_path), "%s/%s", runtime_dir, CONSTELLATION_SOCKET_NAME);
    } else {
        snprintf(socket_path, sizeof(socket_path), "/tmp/%s", CONSTELLATION_SOCKET_NAME);
    }
}

bool constellation_init(struct neowall_state *state) {
    (void)state;
    init_socket_path();

    struct constellation_state *cstate = constellation_get_state();
    cstate->socket_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (cstate->socket_fd < 0) {
        log_error("Failed to create constellation socket: %s", strerror(errno));
        return false;
    }

    unlink(socket_path);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    size_t path_len = strlen(socket_path);
    if (path_len >= sizeof(addr.sun_path)) {
        socket_path[sizeof(addr.sun_path) - 1] = '\0';
        log_warn("Socket path truncated to %zu bytes", sizeof(addr.sun_path) - 1);
    }
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    if (bind(cstate->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("Failed to bind constellation socket to %s: %s", socket_path, strerror(errno));
        close(cstate->socket_fd);
        cstate->socket_fd = -1;
        return false;
    }

    if (listen(cstate->socket_fd, 5) < 0) {
        log_error("Failed to listen on constellation socket: %s", strerror(errno));
        close(cstate->socket_fd);
        cstate->socket_fd = -1;
        return false;
    }

    log_info("Constellation socket listening at %s", socket_path);
    cstate->enabled = true;
    return true;
}

void constellation_cleanup(void) {
    struct constellation_state *cstate = constellation_get_state();
    if (cstate->socket_fd >= 0) {
        close(cstate->socket_fd);
        cstate->socket_fd = -1;
    }
    unlink(socket_path);
}

int constellation_get_socket_fd(void) {
    return constellation_get_state()->socket_fd;
}

static int parse_json_agent_update(const char *json, char *agent_name, size_t name_size,
                                   float *brightness, float *color, bool *active, bool *pulse) {
    const char *type_start = strstr(json, "\"type\"");
    if (!type_start) return -1;

    const char *agent_start = strstr(json, "\"agent\"");
    if (!agent_start) return -1;

    const char *name_start = strchr(agent_start, ':');
    if (!name_start) return -1;
    name_start++;
    while (*name_start && (*name_start == ' ' || *name_start == '"')) name_start++;

    const char *name_end = name_start;
    while (*name_end && *name_end != '"' && *name_end != ',' && *name_end != '}') name_end++;

    size_t name_len = name_end - name_start;
    if (name_len >= name_size) return -1;
    strncpy(agent_name, name_start, name_len);
    agent_name[name_len] = '\0';

    const char *brightness_start = strstr(json, "\"brightness\"");
    if (brightness_start) {
        const char *val_start = strchr(brightness_start, ':');
        if (val_start) {
            *brightness = atof(val_start + 1);
        }
    }

    const char *color_start = strstr(json, "\"color\"");
    if (color_start) {
        const char *arr_start = strchr(color_start, '[');
        if (arr_start) {
            float c[3] = {0, 0, 0};
            int idx = 0;
            const char *p = arr_start + 1;
            while (*p && *p != ']' && idx < 3) {
                while (*p && (*p == ' ' || *p == ',')) p++;
                if (*p && *p != ']') {
                    c[idx++] = atof(p);
                    while (*p && *p != ' ' && *p != ',' && *p != ']') p++;
                }
            }
            color[0] = c[0];
            color[1] = c[1];
            color[2] = c[2];
        }
    }

    const char *active_start = strstr(json, "\"active\"");
    if (active_start) {
        const char *val_start = strchr(active_start, ':');
        if (val_start) {
            *active = (strstr(val_start, "true") != NULL);
        }
    }

    const char *pulse_start = strstr(json, "\"pulse\"");
    if (pulse_start) {
        const char *val_start = strchr(pulse_start, ':');
        if (val_start) {
            *pulse = (strstr(val_start, "true") != NULL);
        }
    }

    return 0;
}

static int parse_json_global(const char *json) {
    const char *theme_start = strstr(json, "\"theme\"");
    if (theme_start) {
        const char *val_start = strchr(theme_start, ':');
        if (val_start) {
            val_start++;
            while (*val_start && (*val_start == ' ' || *val_start == '"')) val_start++;
            if (strncmp(val_start, "minimal", 7) == 0) {
                constellation_set_global_theme(CONSTELLATION_THEME_MINIMAL);
            } else if (strncmp(val_start, "deep_space", 10) == 0) {
                constellation_set_global_theme(CONSTELLATION_THEME_DEEP_SPACE);
            }
        }
    }

    const char *speed_start = strstr(json, "\"animation_speed\"");
    if (speed_start) {
        const char *val_start = strchr(speed_start, ':');
        if (val_start) {
            constellation_set_animation_speed(atof(val_start + 1));
        }
    }

    const char *vis_start = strstr(json, "\"visibility\"");
    if (vis_start) {
        const char *val_start = strchr(vis_start, ':');
        if (val_start) {
            constellation_set_visibility(atof(val_start + 1));
        }
    }

    const char *bg_start = strstr(json, "\"background_color\"");
    if (bg_start) {
        const char *arr_start = strchr(bg_start, '[');
        if (arr_start) {
            float c[3] = {0, 0, 0};
            int idx = 0;
            const char *p = arr_start + 1;
            while (*p && *p != ']' && idx < 3) {
                while (*p && (*p == ' ' || *p == ',')) p++;
                if (*p && *p != ']') {
                    c[idx++] = atof(p);
                    while (*p && *p != ' ' && *p != ',' && *p != ']') p++;
                }
            }
            constellation_set_background_color(c[0], c[1], c[2]);
        }
    }

    return 0;
}

static const char *get_json_type(const char *json) {
    const char *type_start = strstr(json, "\"type\"");
    if (!type_start) return NULL;

    const char *val_start = strchr(type_start, ':');
    if (!val_start) return NULL;
    val_start++;
    while (*val_start && (*val_start == ' ' || *val_start == '"')) val_start++;

    return val_start;
}

static void handle_client_message(int client_fd, const char *json, size_t len) {
    (void)len;

    const char *msg_type = get_json_type(json);
    if (!msg_type) {
        write(client_fd, "{\"type\":\"error\",\"message\":\"missing type\"}\n", 42);
        return;
    }

    if (strncmp(msg_type, "ping", 4) == 0) {
        write(client_fd, "{\"type\":\"pong\",\"version\":\"0.1.0\"}\n", 33);
        return;
    }

    if (strncmp(msg_type, "agent_update", 12) == 0) {
        char name[64] = {0};
        float brightness = CONSTELLATION_BASE_BRIGHTNESS;
        float color[3] = {1.0f, 1.0f, 1.0f};
        bool active = false;
        bool pulse = false;

        if (parse_json_agent_update(json, name, sizeof(name), &brightness, color, &active, &pulse) == 0) {
            constellation_set_agent_brightness(name, brightness);
            constellation_set_agent_color(name, color[0], color[1], color[2]);
            constellation_set_agent_active(name, active);
            constellation_set_agent_pulse(name, pulse);
            write(client_fd, "{\"type\":\"ack\",\"status\":\"ok\"}\n", 30);
        } else {
            write(client_fd, "{\"type\":\"error\",\"message\":\"parse error\"}\n", 37);
        }
        return;
    }

    if (strncmp(msg_type, "global", 6) == 0) {
        parse_json_global(json);
        write(client_fd, "{\"type\":\"ack\",\"status\":\"ok\"}\n", 30);
        return;
    }

    write(client_fd, "{\"type\":\"ack\",\"status\":\"ok\"}\n", 30);
}

void constellation_handle_socket_events(void) {
    struct constellation_state *cstate = constellation_get_state();
    if (!cstate->enabled || cstate->socket_fd < 0) return;

    int client_fd = accept4(cstate->socket_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log_debug("Accept failed on constellation socket: %s", strerror(errno));
        }
        return;
    }

    char buffer[MAX_JSON_SIZE];
    ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        handle_client_message(client_fd, buffer, n);
    }
    close(client_fd);
}
