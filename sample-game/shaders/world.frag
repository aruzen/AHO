#version 450
layout(location=0) in vec3 world;
layout(location=1) in vec3 faceNormal;
layout(location=2) in vec3 albedo;
layout(location=3) flat in int kind;
layout(location=0) out vec4 outputColor;
layout(push_constant) uniform Scene {
    mat4 viewProjection;
    vec4 eyeTime;
    vec4 settings;
    vec4 forward;
    vec4 right;
} scene;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3. - 2. * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + 1), f.x), f.y);
}
vec3 aurora(vec3 d) {
    float az = atan(d.x, -d.z);
    float elevation = asin(clamp(d.y, -1., 1.));
    float t = scene.eyeTime.w;
    vec3 light = vec3(0);
    for (int i = 0; i < 3; ++i) {
        float lane = float(i);
        float wave = .32 + lane * .19 + .13 * sin(az * 2.4 + lane * 1.9 + t * .035) + .06 * sin(az * 5. + lane + t * .07);
        float height = elevation - wave;
        float curtain = exp(-abs(height) * 7.) * smoothstep(-.035, .045, height);
        float folds = .45 + .55 * pow(.5 + .5 * sin(az * 85. + noise(vec2(az * 5., lane + t * .03)) * 15.), 2.);
        float veil = (.6 + .4 * sin(az * 3. + lane * 2.));
        vec3 tint = mix(vec3(.16, .9, .69), vec3(1., .28, .45), smoothstep(.03, .25, height));
        light += tint * curtain * folds * veil * .68;
    }
    return light * smoothstep(.02, .18, d.y);
}
vec3 sky(vec3 d) {
    float night = scene.settings.x;
    vec3 sun = normalize(vec3(-.65, .38, -.65));
    float up = pow(max(d.y, 0.), .48);
    vec3 day = mix(vec3(.83, .85, .68), vec3(.29, .61, .66), up);
    vec3 dark = mix(vec3(.14, .25, .28), vec3(.025, .075, .12), up);
    float sunDot = max(dot(d, sun), 0.);
    day += vec3(1., .66, .28) * pow(sunDot, 18.) * .35;
    day += vec3(1., .92, .66) * smoothstep(.9991, .9995, sunDot) * 1.3;
    vec3 c = mix(day, dark, night);
    // Broad, softly faceted cloud banks.
    vec2 cloudUV = d.xz / max(.13, d.y + .24) * 2.5;
    float cloud = smoothstep(.48, .79, noise(cloudUV + vec2(scene.eyeTime.w * .003, 0)) * .7 + noise(cloudUV * 2.7) * .3);
    c = mix(c, mix(vec3(.96, .93, .78), vec3(.14, .23, .25), night), cloud * smoothstep(.0, .25, d.y) * .45);
    vec2 starUV = vec2(atan(d.x, d.z), asin(d.y)) * 130.;
    vec2 cell = floor(starUV), f = fract(starUV);
    float star = 1. - smoothstep(.005, .065, length(f - vec2(hash(cell + 5), hash(cell + 12))));
    star *= step(.985, hash(cell)) * (.6 + .4 * sin(scene.eyeTime.w * .7 + hash(cell) * 100.));
    c += star * vec3(.75, .9, 1.) * night * smoothstep(0., .2, d.y);
    c += aurora(d) * night * 1.15;
    // Thin luminous trails echo the reference's sky lines.
    float az = atan(d.x, -d.z);
    float trail = pow(max(0., 1. - abs(sin(az * 5.5 + d.y * 7.)) * 320.), 2.);
    c += vec3(.36, .72, .66) * trail * smoothstep(.22, .5, d.y) * .24 * night;
    return c;
}

uint glyph(int c) {
    switch(c) {
        case 65: return 18415150u;
        case 66: return 16301615u;
        case 67: return 31491134u;
        case 68: return 16303663u;
        case 69: return 32554047u;
        case 70: return 1096767u;
        case 71: return 32044094u;
        case 72: return 18415153u;
        case 73: return 32641183u;
        case 74: return 6594844u;
        case 75: return 18128177u;
        case 76: return 32539681u;
        case 77: return 18405233u;
        case 78: return 18667121u;
        case 79: return 15255086u;
        case 80: return 1097263u;
        case 81: return 23385646u;
        case 82: return 18136623u;
        case 83: return 16267326u;
        case 84: return 4329631u;
        case 85: return 15255089u;
        case 86: return 4539953u;
        case 87: return 18732593u;
        case 88: return 18157905u;
        case 89: return 4329809u;
        case 90: return 32575775u;
        case 49: return 14815428u;
        case 50: return 32553487u;
        case 51: return 16267791u;
        case 47: return 1118480u;
        case 45: return 31744u;
        default: return 0u;
    }
}
const int title[11] = int[](87,32,65,32,78,32,68,32,69,32,82);
float titleText(vec2 p) {
    ivec2 cell = ivec2(floor(p));
    if (p.x < 0. || p.y < 0. || cell.y >= 5 || cell.x >= 66) return 0.;
    int column = cell.x % 6;
    return column < 5 ? float((glyph(title[cell.x / 6]) >> uint(column + cell.y * 5)) & 1u) : 0.;
}
const int subtitle[29] = int[](65,32,81,85,73,69,84,32,87,79,82,76,68,32,84,79,32,87,65,76,75,32,84,72,82,79,85,71,72);
float subtitleText(vec2 p) {
    ivec2 cell = ivec2(floor(p));
    if (p.x < 0. || p.y < 0. || cell.y >= 5 || cell.x >= 174) return 0.;
    int column = cell.x % 6;
    return column < 5 ? float((glyph(subtitle[cell.x / 6]) >> uint(column + cell.y * 5)) & 1u) : 0.;
}
const int controls[57] = int[](87,65,83,68,32,87,65,76,75,32,32,32,83,72,73,70,84,32,82,85,78,32,32,32,67,76,73,67,75,32,76,79,79,75,32,32,32,69,83,67,32,82,69,76,69,65,83,69,32,32,32,81,32,81,85,73,84);
float controlsText(vec2 p) {
    ivec2 cell = ivec2(floor(p));
    if (p.x < 0. || p.y < 0. || cell.y >= 5 || cell.x >= 342) return 0.;
    int column = cell.x % 6;
    return column < 5 ? float((glyph(controls[cell.x / 6]) >> uint(column + cell.y * 5)) & 1u) : 0.;
}
const int vistas[63] = int[](49,32,76,65,75,69,32,32,32,50,32,83,78,79,87,32,32,32,51,32,65,85,82,79,82,65,32,32,32,84,32,68,65,89,32,47,32,78,73,71,72,84,32,32,32,72,32,72,73,68,69,32,32,32,70,49,50,32,80,72,79,84,79);
float vistasText(vec2 p) {
    ivec2 cell = ivec2(floor(p));
    if (p.x < 0. || p.y < 0. || cell.y >= 5 || cell.x >= 378) return 0.;
    int column = cell.x % 6;
    return column < 5 ? float((glyph(vistas[cell.x / 6]) >> uint(column + cell.y * 5)) & 1u) : 0.;
}
vec3 hud(vec3 c) {
    if (scene.settings.z < .5) return c;
    vec2 p = gl_FragCoord.xy / vec2(scene.right.w, scene.forward.w) * vec2(1280, 800);
    float top = (1. - smoothstep(0., 125., p.y)) * .16;
    float bottom = smoothstep(700., 800., p.y) * .36;
    c = mix(c, vec3(.035, .1, .12), max(top, bottom));
    float ink = titleText((p - vec2(40, 35)) / 3.);
    ink = max(ink, subtitleText((p - vec2(40, 64)) / 1.35) * .65);
    ink = max(ink, controlsText((p - vec2(40, 743)) / 1.5) * .85);
    ink = max(ink, vistasText((p - vec2(40, 766)) / 1.3) * .6);
    c = mix(c, vec3(.88, .94, .84), ink);
    if (p.x >= 40. && p.x < 79. && p.y >= 89. && p.y < 91.) c = vec3(.78, .89, .69);
    return c;
}

void main() {
    float night = scene.settings.x;
    vec3 view = normalize(world - scene.eyeTime.xyz);
    vec3 c;
    float alpha = 1.;
    if (kind == 2) {
        c = sky(view);
    } else {
        vec3 sun = normalize(vec3(-.65, .75, -.65));
        float diffuse = max(dot(normalize(faceNormal), sun), 0.);
        vec3 lighting = mix(vec3(.51, .68, .66) + diffuse * vec3(.72, .61, .4), vec3(.22, .39, .44) + diffuse * vec3(.22, .32, .29), night);
        c = albedo * lighting;
        if (kind == 1) {
            float t = scene.eyeTime.w;
            vec2 p = world.xz;
            vec3 n = normalize(vec3(sin(p.x * .6 + t * .65) * .018 + cos(p.y * .23 + t * .4) * .026, 1., sin(p.y * .5 - t * .5) * .024));
            vec3 reflected = reflect(view, n);
            float fresnel = .3 + .65 * pow(1. - max(dot(-view, n), 0.), 3.);
            c = mix(mix(vec3(.16, .35, .33), vec3(.055, .15, .17), night), sky(reflected) * .88, fresnel);
            float ripple = pow(max(0., sin(p.y * 2.4 + sin(p.x * .31) * 2. + t * .9)), 26.);
            c += ripple * .045 * mix(vec3(1., .92, .65), vec3(.3, .8, .74), night);
            c += vec3(1., .77, .36) * pow(max(dot(reflected, normalize(vec3(-.65, .38, -.65))), 0.), 180.) * (1. - night) * .7;
        }
        if (kind == 3) {
            c = mix(vec3(.85, .94, .83), vec3(.4, .75, .71), night);
            alpha = .72;
        }
        if (kind == 4) {
            c = albedo * mix(1., .5, night);
            alpha = .18;
        }
        float distance = length(world - scene.eyeTime.xyz);
        float fog = 1. - exp(-distance * mix(.0032, .0046, night));
        vec3 mist = sky(normalize(vec3(view.x, .035, view.z)));
        c = mix(c, mist, clamp(fog, 0., .9));
    }
    // Gentle film response, deliberately retaining the pastel palette.
    c = c / (c + vec3(.65));
    c = pow(c, vec3(.90));
    outputColor = vec4(hud(c), alpha);
}
