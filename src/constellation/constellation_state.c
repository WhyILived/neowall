#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "constellation.h"

static struct constellation_state global_state = {
    .socket_fd = -1,
    .enabled = false,
    .agent_count = 0,
    .global_time = 0.0f,
    .animation_speed = 1.0f,
    .visibility = 1.0f,
    .background_color = {0.0f, 0.0f, 0.0f},
    .theme_id = CONSTELLATION_THEME_MINIMAL,
    .mouse_x = 0.5f,
    .mouse_y = 0.5f,
    .mouse_active = false,
};

static const struct {
    const char *name;
    float color[3];
} default_agents[] = {
    {"WIL",     {0.0f, 0.7f, 0.9f}},   /* Cyan - Orchestrator */
    {"Dubhe",   {0.2f, 0.4f, 0.9f}},   /* Blue - Observer */
    {"Merak",   {0.6f, 0.3f, 0.8f}},   /* Purple - Memory */
    {"Phecda",  {0.2f, 0.7f, 0.4f}},   /* Green - Planner */
    {"Megrez",  {0.9f, 0.7f, 0.2f}},   /* Yellow - Context */
    {"Alioth",  {0.9f, 0.4f, 0.2f}},   /* Orange - Executor */
    {"Alkaid",  {0.2f, 0.8f, 0.7f}},   /* Teal - Coding */
    {"Alcor",   {0.9f, 0.3f, 0.6f}},   /* Pink - Social */
};

#define DEFAULT_AGENT_COUNT (sizeof(default_agents) / sizeof(default_agents[0]))

struct constellation_state *constellation_get_state(void) {
    return &global_state;
}

void constellation_state_init(void) {
    memset(&global_state, 0, sizeof(global_state));
    global_state.socket_fd = -1;
    global_state.enabled = false;
    global_state.agent_count = DEFAULT_AGENT_COUNT;
    global_state.global_time = 0.0f;
    global_state.animation_speed = 1.0f;
    global_state.visibility = 1.0f;
    global_state.background_color[0] = 0.0f;
    global_state.background_color[1] = 0.0f;
    global_state.background_color[2] = 0.0f;
    global_state.theme_id = CONSTELLATION_THEME_MINIMAL;
    global_state.mouse_x = 0.5f;
    global_state.mouse_y = 0.5f;
    global_state.mouse_active = false;

    for (int i = 0; i < global_state.agent_count && i < CONSTELLATION_MAX_AGENTS; i++) {
        struct constellation_agent *agent = &global_state.agents[i];
        snprintf(agent->name, sizeof(agent->name), "%s", default_agents[i].name);
        agent->brightness = CONSTELLATION_BASE_BRIGHTNESS;
        agent->color[0] = default_agents[i].color[0];
        agent->color[1] = default_agents[i].color[1];
        agent->color[2] = default_agents[i].color[2];
        agent->active = false;
        agent->pulse = false;
        agent->target_brightness = CONSTELLATION_BASE_BRIGHTNESS;
    }
}

void constellation_update(float dt) {
    global_state.global_time += dt * global_state.animation_speed;

    for (int i = 0; i < global_state.agent_count && i < CONSTELLATION_MAX_AGENTS; i++) {
        struct constellation_agent *agent = &global_state.agents[i];

        float diff = agent->target_brightness - agent->brightness;
        if (fabsf(diff) > 0.001f) {
            float speed = (diff > 0) ? 0.1f : CONSTELLATION_DECAY_RATE;
            agent->brightness += diff * speed;
        } else {
            agent->brightness = agent->target_brightness;
        }

        agent->brightness = fmaxf(CONSTELLATION_BASE_BRIGHTNESS * 0.5f,
                                  fminf(CONSTELLATION_CAP_BRIGHTNESS, agent->brightness));
    }
}

void constellation_set_agent_brightness(const char *name, float brightness) {
    for (int i = 0; i < global_state.agent_count && i < CONSTELLATION_MAX_AGENTS; i++) {
        if (strcmp(global_state.agents[i].name, name) == 0) {
            global_state.agents[i].target_brightness = brightness;
            return;
        }
    }
}

void constellation_set_agent_color(const char *name, float r, float g, float b) {
    for (int i = 0; i < global_state.agent_count && i < CONSTELLATION_MAX_AGENTS; i++) {
        if (strcmp(global_state.agents[i].name, name) == 0) {
            global_state.agents[i].color[0] = r;
            global_state.agents[i].color[1] = g;
            global_state.agents[i].color[2] = b;
            return;
        }
    }
}

void constellation_set_agent_active(const char *name, bool active) {
    for (int i = 0; i < global_state.agent_count && i < CONSTELLATION_MAX_AGENTS; i++) {
        if (strcmp(global_state.agents[i].name, name) == 0) {
            global_state.agents[i].active = active;
            return;
        }
    }
}

void constellation_set_agent_pulse(const char *name, bool pulse) {
    for (int i = 0; i < global_state.agent_count && i < CONSTELLATION_MAX_AGENTS; i++) {
        if (strcmp(global_state.agents[i].name, name) == 0) {
            global_state.agents[i].pulse = pulse;
            return;
        }
    }
}

void constellation_set_global_theme(int theme_id) {
    global_state.theme_id = theme_id;
}

void constellation_set_animation_speed(float speed) {
    global_state.animation_speed = speed;
}

void constellation_set_visibility(float visibility) {
    global_state.visibility = visibility;
}

void constellation_set_background_color(float r, float g, float b) {
    global_state.background_color[0] = r;
    global_state.background_color[1] = g;
    global_state.background_color[2] = b;
}

void constellation_update_uniforms(float *agent_brightness, float *agent_color,
                                   int *agent_active, float *agent_pulse,
                                   int *agent_count,
                                   float *global_time, float *animation_speed,
                                   float *visibility, float *background_color,
                                   int *theme_id) {
    *agent_count = global_state.agent_count;

    for (int i = 0; i < global_state.agent_count && i < CONSTELLATION_MAX_AGENTS; i++) {
        agent_brightness[i] = global_state.agents[i].brightness * global_state.visibility;
        agent_color[i * 3 + 0] = global_state.agents[i].color[0];
        agent_color[i * 3 + 1] = global_state.agents[i].color[1];
        agent_color[i * 3 + 2] = global_state.agents[i].color[2];
        agent_active[i] = global_state.agents[i].active ? 1 : 0;
        agent_pulse[i] = global_state.agents[i].pulse ? 1.0f : 0.0f;
    }

    *global_time = global_state.global_time;
    *animation_speed = global_state.animation_speed;
    *visibility = global_state.visibility;
    background_color[0] = global_state.background_color[0];
    background_color[1] = global_state.background_color[1];
    background_color[2] = global_state.background_color[2];
    *theme_id = global_state.theme_id;
}
