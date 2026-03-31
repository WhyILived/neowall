#ifndef NEOWALL_CONSTELLATION_H
#define NEOWALL_CONSTELLATION_H

#include <stdbool.h>
#include <stdint.h>

struct neowall_state;

#define CONSTELLATION_MAX_AGENTS 32
#define CONSTELLATION_SOCKET_NAME "neowall.sock"

#define CONSTELLATION_BASE_BRIGHTNESS 0.3f
#define CONSTELLATION_CAP_BRIGHTNESS 1.0f
#define CONSTELLATION_DECAY_RATE 0.02f

#define CONSTELLATION_THEME_MINIMAL 0
#define CONSTELLATION_THEME_DEEP_SPACE 1

struct constellation_agent {
    char name[64];
    float brightness;
    float color[3];
    bool active;
    bool pulse;
    float target_brightness;
};

struct constellation_state {
    int socket_fd;
    bool enabled;

    int agent_count;
    struct constellation_agent agents[CONSTELLATION_MAX_AGENTS];

    float global_time;
    float animation_speed;
    float visibility;
    float background_color[3];
    int theme_id;

    float mouse_x;
    float mouse_y;
    bool mouse_active;
};

struct constellation_state *constellation_get_state(void);

bool constellation_init(struct neowall_state *state);

void constellation_cleanup(void);

int constellation_get_socket_fd(void);

void constellation_handle_socket_events(void);

void constellation_update_uniforms(float *agent_brightness, float *agent_color,
                                   int *agent_active, float *agent_pulse,
                                   int *agent_count,
                                   float *global_time, float *animation_speed,
                                   float *visibility, float *background_color,
                                   int *theme_id);

void constellation_update(float dt);

void constellation_state_init(void);

void constellation_set_agent_brightness(const char *name, float brightness);

void constellation_set_agent_color(const char *name, float r, float g, float b);

void constellation_set_agent_active(const char *name, bool active);

void constellation_set_agent_pulse(const char *name, bool pulse);

void constellation_set_global_theme(int theme_id);

void constellation_set_animation_speed(float speed);

void constellation_set_visibility(float visibility);

void constellation_set_background_color(float r, float g, float b);

const char *constellation_get_socket_path(void);

#endif /* NEOWALL_CONSTELLATION_H */
