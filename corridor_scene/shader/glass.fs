#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;

uniform sampler2D uColorTex;
uniform bool      uIsSunPatch;
uniform vec4      uColor;
uniform vec3      viewPos;
uniform vec3      uSunDir;   // normalize(shadowLightDir) -- 광원->씬 방향

const float PI = 3.14159265358979;

void main()
{
    if (uIsSunPatch)
    {
        // 창문(z = -6.75) 기준 방 안쪽 깊이 — 창문 프레임 돌출 보정으로 -6.80 기준
        float depth = max(0.0, FragPos.z + 6.80);

        // 빛 각도에 따른 x 보정 — 깊을수록 패치가 빛 방향으로 미세하게 이동
        float x_adj = FragPos.x - depth * (uSunDir.x / uSunDir.z);

        // 창문 4개 가우시안 합산 (sigma=1.0m)
        //   → 2.5m 간격 인접 창문은 자연스럽게 보간
        //   → 5m 떨어진 반대 클러스터는 거의 영향 없음
        float sigma = 1.0;
        float xFac  = 0.0;
        xFac += exp(-0.5 * pow((x_adj + 5.0) / sigma, 2.0));
        xFac += exp(-0.5 * pow((x_adj + 2.5) / sigma, 2.0));
        xFac += exp(-0.5 * pow((x_adj - 2.5) / sigma, 2.0));
        xFac += exp(-0.5 * pow((x_adj - 5.0) / sigma, 2.0));
        xFac = clamp(xFac, 0.0, 1.0);

        // 방 측벽(x = ±6.75) 근처에서 sun patch를 fade out → 벽 경계 빛 샘 방지
        // 창문 없는 벽 구간(|x|>5.70)에서 서서히 0으로 수렴
        float wallFade = smoothstep(6.75, 5.50, abs(FragPos.x));
        xFac *= wallFade;

        // 지수 감쇄: 창문 바로 앞이 가장 밝고 방 안으로 멀리 퍼지며 서서히 흐려짐
        //   depth=0m: 1.0 / 2m: 0.53 / 4m: 0.28 / 7m: 0.10
        float fade = exp(-depth * 0.60);

        // 가산 블렌딩(GL_ONE, GL_ONE) — rgb 만 출력
        FragColor = vec4(uColor.rgb * xFac * fade, 1.0);
        return;
    }

    // camera -> fragment 방향 벡터를 equirectangular UV 로 변환
    vec3 dir = normalize(FragPos - viewPos);
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * PI);
    float v = 0.5 + asin(clamp(dir.y, -1.0, 1.0)) / PI;
    vec3 sky = texture(uColorTex, vec2(u, v)).rgb;
    FragColor = vec4(sky, 0.97);
}
