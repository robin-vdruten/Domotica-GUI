#type vertex
#version 460 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
	v_TexCoord = a_TexCoord;
	gl_Position = vec4(a_Position, 0.0, 1.0);
}

#type fragment
#version 460 core

// Created by anatole duprat - XT95/2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// https://www.shadertoy.com/view/MdX3zr

layout (location = 0) out vec4 fragColor;

layout(location = 0) in vec2 v_TexCoord;

layout(location = 0) uniform float u_Time;
layout(location = 1) uniform vec2 u_Resolution;


#define PI 3.1415926535
#define T u_Time*3.
#define PELLET_SIZE 1./16.
#define PELLET_NUM 2
#define THICKNESS 0.13
#define RADIUS 0.7
//#define COUNTER_ROT // counter rotation that keeps the arc at the same place
#define SMOOTH_MOT // smooth motion of pellets
#define SMOOTH_SHP // smooth shapes

float sdArc( in vec2 p, in float a, in float ra, float rb ) // By iq: https://www.shadertoy.com/view/wl23RK
{
    a *= PI;
    vec2 sc = vec2( sin(a),cos(a) );
    p.x = abs(p.x);
    return ((sc.y*p.x>sc.x*p.y) ? length(p-sc*ra) : 
                                  abs(length(p)-ra)) - rb;
}

mat2 rot( float a )
{
    a *= PI;
    float s = sin(a), c = cos(a);
    return mat2( c,-s,s,c );
}

float s( float x )
{
#ifdef SMOOTH_MOT
    return smoothstep( 0.,1.,x );
#endif
    return x;
}

float sminCubic( float a, float b, float k ) // By iq: https://iquilezles.org/articles/smin/
{
    float h = max( k-abs(a-b), .0 )/k;
    return min( a, b ) - h*h*h*k*(1./6.);
}

vec3 pal(float x)
{
    return mix(vec3(0.8), vec3(1.0), x); // grey to white
}

float f(float x) {
    return -2.*PELLET_SIZE*x;
}

float dist(vec2 p){
#ifdef COUNTER_ROT
    p *= rot(-f(T-.5));
#endif
    int n = PELLET_NUM;
    float N = float(n);

    float d1 = sdArc( p*rot( f(floor(T)) + 1. ), 
                      .5 - PELLET_SIZE, 
                      RADIUS, 
                      THICKNESS);
    float d2 = 9e9;
    for (int i = 0; i < n; i++) {
        float j = float(i);
        float t = s( fract((T + j)/N) );
        float a = mix( -.5, .5 - f(1.), t) + f(T);
        d2 = min( sdArc( p * rot(a),
                         PELLET_SIZE,
                         RADIUS,
                         THICKNESS), d2);
    }
#ifdef SMOOTH_SHP
    float r = abs( length(p) - RADIUS ) - THICKNESS; // sdf of ring containing the arcs 
    float d = sminCubic( d1, d2, .2 );
    return max( d, r );
#endif
    return min(d1, d2);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord/u_Resolution.xy - .5;
    vec2 p = (2.*fragCoord - u_Resolution.xy)/u_Resolution.y;

    float d = dist( p ); // shape
    float m = smoothstep( .01,.0,d );

    float d1 = dist( p + vec2(0.,.15) ); // shadow
    float s = smoothstep( .2,-.4,d1 );

    m = max(s,m); // combine shadow and shape

    vec3 col = m*pal( p.x - p.y + .5 ); // color shape and shadow
    col = mix(vec3(0.17), col, m); // lighter ImGui gray background
    col *= 1. - 1.5 * dot(uv,uv); // vignette
    fragColor = vec4(col,1.);
}