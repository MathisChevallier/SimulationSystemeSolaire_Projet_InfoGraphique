#version 130
precision mediump float;

uniform vec3 uMtlColor;
uniform vec4 uMtlCts;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uCameraPosition;

varying vec3 vary_normal;
varying vec4 vary_world_position;

void main()
{
	vec3 normal   = normalize(vary_normal);
	vec3 lightDir = normalize(uLightPos - vary_world_position.xyz);
	vec3 V        = normalize(uCameraPosition - vary_world_position.xyz);
	vec3 R        = reflect(-lightDir, normal);
	
	vec3 ambient  = uMtlCts.x * uMtlColor * uLightColor;
	vec3 diffuse  = uMtlCts.y * max(0.0, dot(normal, lightDir)) * uMtlColor * uLightColor;
	vec3 specular = uMtlCts.z * pow(max(0.0, dot(R, V)), uMtlCts.w) * uLightColor;
    
	
	gl_FragColor  = vec4(ambient + diffuse + specular, 1.0);
	
}
