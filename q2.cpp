#include <Windows.h>
#include <iostream>
#include <GL/glew.h>
#include <GL/GL.h>
#include <GL/freeglut.h>

#define GLFW_INCLUDE_GLU
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <vector>

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace glm;

int Width = 512;
int Height = 512;
std::vector<float> OutputImage;

struct Ray {
	vec3 origin;
	vec3 direction;

	Ray(const vec3& o, const vec3& d) : origin(o), direction(d) {}
};

struct Intersection {
	bool hit;
	float distance;
	vec3 point;
	vec3 normal;
	class Material* material;
};

class Material {
public:
	vec3 ka, kd, ks;
	float power;

	Material(vec3 a, vec3 d, vec3 s, float p) : ka(a), kd(d), ks(s), power(p) {}
};

class Surface {
public:
	Material* material;
	virtual ~Surface() {}
	virtual Intersection intersect(const Ray& ray) const = 0;
};

class Plane : public Surface {
public:
	vec3 normal;
	float d;

	Plane(vec3 n, float d_, Material* m) : normal(glm::normalize(n)), d(d_) {
		material = m;
	}

	Intersection intersect(const Ray& ray) const override {
		float denom = glm::dot(normal, ray.direction);
		if (fabs(denom) > 1e-6) {
			float t = -(glm::dot(normal, ray.origin) + d) / denom;
			if (t >= 0.001f) {
				vec3 point = ray.origin + t * ray.direction;
				return { true, t, point, normal, material };
			}
		}
		return { false };
	}
};

class Sphere : public Surface {
public:
	vec3 center;
	float radius;

	Sphere(vec3 c, float r, Material* m) : center(c), radius(r) {
		material = m;
	}

	Intersection intersect(const Ray& ray) const override {
		vec3 oc = ray.origin - center;
		float a = dot(ray.direction, ray.direction);
		float b = 2.0f * dot(oc, ray.direction);
		float c = dot(oc, oc) - radius * radius;
		float discriminant = b * b - 4 * a * c;

		if (discriminant < 0) return { false };
		float t = (-b - glm::sqrt(discriminant)) / (2.0f * a);
		if (t > 0.001f) {
			vec3 point = ray.origin + t * ray.direction;
			vec3 normal = normalize(point - center);
			return { true, t, point, normal, material };
		}
		return { false };
	}
};

class Camera {
public:
	vec3 eye;
	vec3 u, v, w;
	float l = -0.1f, r = 0.1f, b = -0.1f, t = 0.1f, d = 0.1f;

	Camera(vec3 e, vec3 u_, vec3 v_, vec3 w_) : eye(e), u(u_), v(v_), w(w_) {}

	Ray getRay(int i, int j) const {
		float su = l + (r - l) * (i + 0.5f) / Width;
		float sv = b + (t - b) * (j + 0.5f) / Height;
		return { eye, normalize(vec3(su, sv, -d)) };
	}
};

vec3 lightPos(-4, 4, -3);
vec3 lightColor(1.0f); // white

bool isInShadow(const vec3& point, const std::vector<Surface*>& scene) {
	vec3 dirToLight = normalize(lightPos - point);
	float lightDist = length(lightPos - point);
	Ray shadowRay(point + dirToLight * 1e-2f, dirToLight); // 오프셋 증가

	for (auto s : scene) {
		Intersection isect = s->intersect(shadowRay);
		if (isect.hit && isect.distance > 1e-3f && isect.distance < lightDist) {
			return true;
		}
	}
	return false;
}


vec3 shade(const Intersection& hit, const Ray& ray, const std::vector<Surface*>& scene) {
	Material* mat = hit.material;
	vec3 ambient = mat->ka * lightColor;

	if (isInShadow(hit.point, scene)) {
		return ambient;
	}

	vec3 l = normalize(lightPos - hit.point);
	vec3 v = normalize(-ray.direction);
	vec3 n = normalize(hit.normal);
	vec3 h = normalize(l + v);

	float diff = std::max(dot(n, l), 0.0f);
	float spec = pow(std::max(dot(n, h), 0.0f), mat->power);

	vec3 diffuse = mat->kd * diff * lightColor;
	vec3 specular = mat->ks * spec * lightColor;

	return ambient + diffuse + specular;
}


void render() {
	OutputImage.clear();

	// Materials
	Material* mPlane = new Material(vec3(0.2), vec3(1.0), vec3(0), 0);
	Material* mS1 = new Material(vec3(0.2, 0, 0), vec3(1, 0, 0), vec3(0), 0);
	Material* mS2 = new Material(vec3(0, 0.2, 0), vec3(0, 0.5, 0), vec3(0.5), 32);
	Material* mS3 = new Material(vec3(0, 0, 0.2), vec3(0, 0, 1), vec3(0), 0);

	// Scene
	std::vector<Surface*> scene;
	scene.push_back(new Sphere(vec3(-4, 0, -7), 1.0f, mS1));
	scene.push_back(new Sphere(vec3(0, 0, -7), 2.0f, mS2));
	scene.push_back(new Sphere(vec3(4, 0, -7), 1.0f, mS3));
	scene.push_back(new Plane(vec3(0, 1, 0), 2, mPlane));

	Camera* c = new Camera(vec3(0, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1));

	for (int j = 0; j < Height; ++j) {
		for (int i = 0; i < Width; ++i) {
			Ray ray = c->getRay(i, j);

			Intersection closest;
			closest.distance = FLT_MAX;
			for (auto s : scene) {
				Intersection isect = s->intersect(ray);
				if (isect.hit && isect.distance < closest.distance) {
					closest = isect;
				}
			}

			vec3 color(0.0f);
			if (closest.hit) {
				color = shade(closest, ray, scene);

			}

			float gamma = 1.0f / 2.2f;
			vec3 corrected = pow(clamp(color, 0.0f, 1.0f), vec3(gamma));
			OutputImage.push_back(corrected.r);
			OutputImage.push_back(corrected.g);
			OutputImage.push_back(corrected.b);

		}
	}

	for (auto s : scene) delete s;
	delete mPlane; delete mS1; delete mS2; delete mS3;
	delete c;
}

void resize_callback(GLFWwindow*, int nw, int nh) {
	Width = nw;
	Height = nh;
	glViewport(0, 0, nw, nh);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, static_cast<double>(Width), 0.0, static_cast<double>(Height), 1.0, -1.0);
	OutputImage.reserve(Width * Height * 3);
	render();
}

int main(int argc, char* argv[]) {
	GLFWwindow* window;
	if (!glfwInit()) return -1;
	window = glfwCreateWindow(Width, Height, "Phong Ray Tracer", NULL, NULL);
	if (!window) { glfwTerminate(); return -1; }
	glfwMakeContextCurrent(window);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glfwSetFramebufferSizeCallback(window, resize_callback);
	resize_callback(NULL, Width, Height);

	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawPixels(Width, Height, GL_RGB, GL_FLOAT, &OutputImage[0]);
		glfwSwapBuffers(window);
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GL_TRUE);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
