#version 130
precision mediump float; //Medium precision for float. highp and smallp can also be used

varying vec4 varyColor; //Sometimes we use "out" instead of "varying". "out" should be used in later version of GLSL.

uniform sampler2D uTexture;

varying vec2 vary_uv;

uniform vec3 uMtlColor;
uniform vec4 uMtlCts;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uCameraPosition;

varying vec3 vary_normal;
varying vec4 vary_world_position;

//We still use varying because OpenGLES 2.0 (OpenGL Embedded System, for example for smartphones) does not accept "in" and "out"

void main()
{
	vec3 normal   = normalize(vary_normal);
	vec3 lightDir = normalize(uLightPos - vary_world_position.xyz);
	vec3 V        = normalize(uCameraPosition - vary_world_position.xyz);
	vec3 R        = reflect(-lightDir, normal);
	
	vec3 ambient  = uMtlCts.x * uMtlColor * uLightColor;
	vec3 diffuse  = uMtlCts.y * max(0.0, dot(normal, lightDir)) * uMtlColor * uLightColor;
	vec3 specular = uMtlCts.z * pow(max(0.0, dot(R, V)), uMtlCts.w) * uLightColor;
    
	vec4 color =  vec4(ambient + diffuse + specular, 1.0) * texture2D(uTexture, vary_uv) ;
      gl_FragColor = color;
      //gl_FragColor = texture2D(uTexture, vary_uv);
}
