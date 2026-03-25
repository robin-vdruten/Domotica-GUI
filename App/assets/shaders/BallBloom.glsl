#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in float a_TexIndex;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

out vec2 v_TexCoord;

void main()
{
	v_TexCoord = a_TexCoord * 2.0 - 1.0;
	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform vec4 u_Color;
uniform vec4 u_ColorSecondary;
uniform float u_AlphaCutoff;

uniform float u_Time;
uniform int   u_ParticleCount;
uniform float u_MaxRadius;
uniform float u_Speed;
uniform float u_Size;
uniform float u_Spread;
uniform float u_Seed;

uniform float u_CoreRadius;
uniform float u_GlowFalloff;

// --- New! Ball's velocity from C++ ---
uniform vec2 u_Velocity;

const int MAX_PARTICLES = 64;

float hash(float x) { return fract(sin(x) * 43758.5453); }

void main() {
    vec2 uv = v_TexCoord;
    vec3 totalColor = vec3(0.0);
    float totalIntensity = 0.0;
    int count = clamp(u_ParticleCount, 0, MAX_PARTICLES);

    // --- 1. Calculate Particle Trail ---
    
    // Get the direction opposite to the ball's movement. This is the center of our trail.
    vec2 trailDir = normalize(-u_Velocity);

    // If the ball isn't moving, trailDir will be (0,0). Avoid this.
    if (length(u_Velocity) < 0.01) {
        trailDir = vec2(-1.0, 0.0); // Default to shooting left
    }

    // Get the base angle for the trail from its direction vector.
    float baseAngle = atan(trailDir.y, trailDir.x);

    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (i >= count) break;
        float fi = float(i);
        float rndA = hash(fi * 12.9898 + u_Seed);
        float rndB = hash(fi * 78.233 + u_Seed);

        vec3 particleColor = (rndA > 0.5) ? u_Color.rgb : u_ColorSecondary.rgb;

        // --- New Trail Logic ---
        // Instead of a full 360 degrees, offset from the base angle by the spread amount.
        float angle = baseAngle + (rndB - 0.5) * u_Spread;
        vec2 dir = vec2(cos(angle), sin(angle));
        // ---

        float phase = fract(u_Time * u_Speed * (0.7 + 0.6 * rndB) + rndA);
        float radius = u_CoreRadius + phase * (u_MaxRadius - u_CoreRadius);
        vec2 pos = dir * radius;
        
        vec2 vecToParticle = uv - pos;
        float distSq = dot(vecToParticle, vecToParticle);
        float tailStretch = dot(normalize(vecToParticle), dir);
        distSq *= 1.0 - 0.95 * smoothstep(0.0, 1.0, tailStretch);

        float lifeFade = pow(1.0 - phase, 2.0);
        float contrib = exp(-distSq / (u_Size * u_Size)) * lifeFade;
        
        totalColor += particleColor * contrib;
        totalIntensity += contrib;
    }

    // --- 2. Calculate Sun Intensity (Unchanged) ---
    float distFromCenter = length(uv);
    float sunGlow = smoothstep(u_CoreRadius + (u_GlowFalloff * 0.1), u_CoreRadius, distFromCenter);
    float sunCore = smoothstep(u_CoreRadius, u_CoreRadius - 0.01, distFromCenter);
    float sunIntensity = sunGlow + sunCore * 1.5;

    // --- 3. Combine and Discard (Unchanged) ---
    totalColor += u_Color.rgb * sunIntensity;
    totalIntensity += sunIntensity;

    if (totalIntensity < u_AlphaCutoff) {
        discard;
    }
    
    // --- 4. Final Color with Premultiplied Alpha (Unchanged) ---
    float finalAlpha = clamp(totalIntensity, 0.0, 1.0);
    vec3 finalColor = totalColor / (totalColor + vec3(1.0)); // Tonemapping
    
    o_Color = vec4(finalColor * finalAlpha, finalAlpha);
}