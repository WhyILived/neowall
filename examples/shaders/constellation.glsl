#define MAX_AGENTS 8
#define PI 3.14159265359

vec2 star_positions[MAX_AGENTS];
float star_brightness[MAX_AGENTS];
vec3 star_color[MAX_AGENTS];
float star_active[MAX_AGENTS];

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

float star_glow(vec2 uv, vec2 pos, float radius, float brightness) {
    float d = length(uv - pos);
    float core = smoothstep(radius, 0.0, d);
    float glow = exp(-d * 10.0) * brightness;
    return core + glow * 0.5;
}

vec3 draw_star(vec2 uv, vec2 pos, float brightness, vec3 color, float twinkle) {
    float d = length(uv - pos);
    float core = smoothstep(0.006, 0.0, d);
    float rays = 0.0;
    if (brightness > 0.3) {
        vec2 dir = normalize(uv - pos + vec2(0.001));
        float angle = atan(dir.y, dir.x);
        rays = pow(abs(sin(angle * 3.0 + iTime * twinkle * 2.0)), 12.0) * exp(-d * 6.0) * brightness * 0.25;
    }
    float glow = exp(-d * 12.0) * brightness;
    float star = core + glow + rays;
    return color * star * brightness;
}

vec3 draw_line(vec2 uv, vec2 a, vec2 b, float brightness, vec3 color) {
    float d = sdSegment(uv, a, b);
    float line = smoothstep(0.002, 0.0, d);
    float glow = exp(-d * 80.0) * brightness * 0.4;
    return color * (line + glow) * brightness;
}

vec2 attract_repel(vec2 star_pos, vec2 mouse, float brightness) {
    vec2 to_star = star_pos - mouse;
    float dist = length(to_star);
    if (dist < 0.001) return vec2(0.0);
    vec2 dir = normalize(to_star + vec2(0.001));
    float repel = exp(-dist * 4.0) * 0.015 * brightness;
    float attract = exp(-dist * 12.0) * 0.003;
    return dir * (attract - repel);
}

void init_stars(void) {
    star_positions[0] = vec2(0.60, 0.35);
    star_positions[1] = vec2(0.55, 0.38);
    star_positions[2] = vec2(0.50, 0.40);
    star_positions[3] = vec2(0.45, 0.42);
    star_positions[4] = vec2(0.40, 0.45);
    star_positions[5] = vec2(0.33, 0.50);
    star_positions[6] = vec2(0.30, 0.55);
    star_positions[7] = vec2(0.35, 0.52);

    star_brightness[0] = 0.6;
    star_brightness[1] = 0.5;
    star_brightness[2] = 0.5;
    star_brightness[3] = 0.4;
    star_brightness[4] = 0.7;
    star_brightness[5] = 0.5;
    star_brightness[6] = 0.8;
    star_brightness[7] = 0.3;

    star_color[0] = vec3(0.0, 0.7, 0.9);
    star_color[1] = vec3(0.2, 0.4, 0.9);
    star_color[2] = vec3(0.6, 0.3, 0.8);
    star_color[3] = vec3(0.9, 0.7, 0.2);
    star_color[4] = vec3(0.9, 0.4, 0.2);
    star_color[5] = vec3(0.2, 0.8, 0.7);
    star_color[6] = vec3(0.9, 0.3, 0.6);
    star_color[7] = vec3(0.5, 0.5, 0.6);

    for (int i = 0; i < MAX_AGENTS; i++) {
        star_active[i] = (mod(iTime, 10.0) > float(i) * 0.5) ? 1.0 : 0.3;
    }
}

vec2 apply_mouse(vec2 base_pos, int idx) {
    vec2 mouse_uv = iMouse.xy / iResolution.xy;
    mouse_uv.x *= iResolution.x / iResolution.y;
    float dist = length(base_pos - mouse_uv);
    float mag = 1.0 + exp(-dist * 10.0) * 0.4 * star_brightness[idx];
    vec2 offset = attract_repel(base_pos, mouse_uv, star_brightness[idx]);
    return base_pos * mag + offset;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float aspect = iResolution.x / iResolution.y;
    uv.x *= aspect;

    init_stars();

    vec3 bg = vec3(0.005, 0.005, 0.02);
    vec3 color = bg;

    for (int i = 0; i < MAX_AGENTS; i++) {
        float brightness = star_brightness[i] * star_active[i];
        if (brightness < 0.1) continue;

        vec2 pos = apply_mouse(star_positions[i], i);
        pos.x *= aspect;

        color += draw_star(uv, pos, brightness, star_color[i], star_brightness[i]);

        if (star_active[i] > 0.5 && brightness > 0.3) {
            for (int j = i + 1; j < MAX_AGENTS; j++) {
                if (star_active[j] > 0.5) {
                    float other_brightness = star_brightness[j] * star_active[j];
                    vec2 other_pos = apply_mouse(star_positions[j], j);
                    other_pos.x *= aspect;
                    float line_brightness = (brightness + other_brightness) * 0.4;
                    color += draw_line(uv, pos, other_pos, line_brightness, star_color[i] * 0.4);
                }
            }
        }
    }

    if (iMouse.z > 0.0) {
        vec2 mouse_uv = iMouse.xy / iResolution.xy;
        mouse_uv.x *= aspect;
        float cursor_glow = exp(-length(uv - mouse_uv) * 4.0) * 0.2;
        color += vec3(0.3, 0.5, 1.0) * cursor_glow;
    }

    float vignette = 1.0 - length(uv - vec2(aspect * 0.5, 0.5)) * 0.4;
    color *= vignette;

    color = pow(color, vec3(1.0 / 2.2));
    fragColor = vec4(color, 1.0);
}
