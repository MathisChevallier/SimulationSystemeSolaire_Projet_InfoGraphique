/*
* ________________________________________Computer graphics project - Polytech Paris-Saclay - Juin 2022___________________________________________________________
*
* @author: MARTIN Hugues, CHEVALLIER Mathis (ET3)
*
* @title: ------------------------------CRASH D'ASTEROIDE------------------------------
*
* @Scene: Animated graphic scene, representing the collision of an asteroid with the sun, which brushes the earth on its way
*
* Design of a solar system, light management, asteroid trajectory, animation of all celestial bodies...
*___________________________________________________________________________________________________________________________________________________________
*/


//SDL Libraries
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_image.h>

//OpenGL Libraries
#include <GL/glew.h>
#include <GL/gl.h>

//GML libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "logger.h"
#include <stack>
#include <vector>

#include "Sphere.h"

#define WIDTH     800
#define HEIGHT    800
#define FRAMERATE 60
#define TIME_PER_FRAME_MS  (1.0f/FRAMERATE * 1e3)
#define INDICE_TO_PTR(x) ((void*)(x))

struct Material {
    glm::vec3 color;
    float ka;
    float kd;
    float ks;
    float alpha;
};

struct Light {
    glm::vec3 position;
    glm::vec3 color;
};

//Objects
struct GameObject {
    GLuint vboID = 0;
    GLuint texture = 0;
    Geometry* geometry = nullptr;
    Material sphereMtl;
    Light light;
    glm::mat4 propagatedMatrix = glm::mat4(1.0f);
    glm::mat4 localMatrix = glm::mat4(1.0f);
    std::vector<GameObject*> children;
};
//Draw each Object, this function displays the planets taking into account the lightand its shadows
void drawSphere(GameObject& go, Shader* shader, std::stack<glm::mat4>& matrices) {


    glm::mat4 camera(1.0f);
    glm::vec3 cameraPosition(0.0f, 800.0f, 1000.0f);
    camera = glm::translate(camera, cameraPosition);

    glm::mat4 projection = glm::perspective(45.0f, WIDTH / (float)HEIGHT, 0.01f, 1000.0f);
    glm::mat4 view = glm::inverse(camera); //See glm::lookAt also if you want to define the camera in other ways
    glm::mat4 model(1.0f);
    model = glm::translate(go.propagatedMatrix, glm::vec3(0.0f, 0.0f, 0.0f)); //Warning: We passed from left-handed world coordinate to right-handed world coordinate due to glm::perspective

    glm::mat3 invModel3x3 = glm::inverse(glm::mat3(model));

    matrices.push(matrices.top() * go.propagatedMatrix);      //Push matrices with propagated matrix
    glm::mat4 mvp = matrices.top() * go.localMatrix;           //Set value of uMVP

    glUseProgram(shader->getProgramID());
    {
        glBindBuffer(GL_ARRAY_BUFFER, go.vboID);

        //VBO:
        //Position
        GLint vPosition = glGetAttribLocation(shader->getProgramID(), "vPosition");
        glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);
        //Colors start at 0
        glEnableVertexAttribArray(vPosition);

        GLint vNormal = glGetAttribLocation(shader->getProgramID(), "vNormal");
        glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 0, INDICE_TO_PTR(go.geometry->getNbVertices() * 3 * sizeof(float)));
        glEnableVertexAttribArray(vNormal);

        //UV
        GLint vUV = glGetAttribLocation(shader->getProgramID(), "vUV");
        glVertexAttribPointer(vUV, 2, GL_FLOAT, GL_FALSE, 0, INDICE_TO_PTR(go.geometry->getNbVertices() * (3 + 3) * (sizeof(float))));
        glEnableVertexAttribArray(vUV);

        //Transformation
        GLint uMVP = glGetUniformLocation(shader->getProgramID(), "uMVP");
        GLint uModel = glGetUniformLocation(shader->getProgramID(), "uModel");
        GLint uInvModel3x3 = glGetUniformLocation(shader->getProgramID(), "uInvModel3x3");
        glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix3fv(uInvModel3x3, 1, GL_FALSE, glm::value_ptr(invModel3x3));

        //Uniform for fragment shader
        GLint uMtlColor = glGetUniformLocation(shader->getProgramID(), "uMtlColor");
        GLint uMtlCts = glGetUniformLocation(shader->getProgramID(), "uMtlCts");
        GLint uLightPos = glGetUniformLocation(shader->getProgramID(), "uLightPos");
        GLint uLightColor = glGetUniformLocation(shader->getProgramID(), "uLightColor");
        GLint uCameraPosition = glGetUniformLocation(shader->getProgramID(), "uCameraPosition");
        glUniform3f(uMtlColor, 1, 1, 1);
        glUniform4f(uMtlCts, go.sphereMtl.ka, go.sphereMtl.kd, go.sphereMtl.ks, go.sphereMtl.alpha);
        glUniform3f(uLightPos, 0, 0, 0);
        glUniform3f(uLightColor, 1, 1, 1);
        glUniform3f(uCameraPosition, 0, 0, 0);

        glActiveTexture(GL_TEXTURE0); //Active Texture0. This is not mandatory for only one
        glBindTexture(GL_TEXTURE_2D, go.texture); //The binding is done on GL_Texture0
        GLint uTexture = glGetUniformLocation(shader->getProgramID(), "uTexture");
        glUniform1i(uTexture, 0);

        //Draw the triangle with nbVertices
        glDrawArrays(GL_TRIANGLES, 0, go.geometry->getNbVertices());

        glBindTexture(GL_TEXTURE_2D, 0); //In fact you do not really need this if you pay

    }
    glUseProgram(0);

    for (int i = 0; i < go.children.size(); i++)
        drawSphere(*(go.children[i]), shader, matrices);

    //Remove the last matrix
    matrices.pop();
}

void createTexture(GLuint texture, SDL_Surface* img) {

    //Convert to an RGBA8888 surface
    SDL_Surface* rgbImg = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_RGBA32, 0);
    //Delete the old surface
    SDL_FreeSurface(img);

    glBindTexture(GL_TEXTURE_2D, texture);
    {
        //All the following parameters are default parameters. Sampler (using glGenSampler,
        //Bilinear filtering is enough when the texture is "far" (min_filter : you have to
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        //Repeat the texture if needed. Texture coordinates go from (0.0, 0.0) (bottom-left)
            //S is "x coordinate" and T "y coordinate"
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        //Send the data. Here you have to pay attention to what is the format of the sending
            //We set the width, the height and the pixels data of this texture (we know here that
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg->w, rgbImg->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg->pixels);
        //Generate mipmap. See figure 2 for mipmap description
        //Optional but recommended
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    SDL_FreeSurface(rgbImg); //Delete the surface at the end of the program
}


int main(int argc, char* argv[])
{
    ////////////////////////////////////////
    //SDL2 / OpenGL Context initialization :
    ////////////////////////////////////////

    //Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
    {
        ERROR("The initialization of the SDL failed : %s\n", SDL_GetError());
        return 0;
    }

    //Create a Window
    SDL_Window* window = SDL_CreateWindow("VR Camera",
        SDL_WINDOWPOS_UNDEFINED,               //X Position
        SDL_WINDOWPOS_UNDEFINED,               //Y Position
        WIDTH, HEIGHT,                         //Resolution
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN); //Flags (OpenGL + Show)

//Initialize OpenGL Version (version 3.0)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    //Initialize the OpenGL Context (where OpenGL resources (Graphics card resources) lives)
    SDL_GLContext context = SDL_GL_CreateContext(window);

    //Tells GLEW to initialize the OpenGL function with this version
    glewExperimental = GL_TRUE;
    glewInit();


    //Start using OpenGL to draw something on screen
    glViewport(0, 0, WIDTH, HEIGHT); //Draw on ALL the screen

    //The OpenGL background color (RGBA, each component between 0.0f and 1.0f)
    glClearColor(0.0, 0.0, 0.0, 1.0); //Full Black

    glEnable(GL_DEPTH_TEST); //Active the depth test

     //Load the file (png)
    SDL_Surface* imgSun = IMG_Load("./Images/2k_sun.png");
    SDL_Surface* imgEarth = IMG_Load("./Images/2k_earth_daymap.png");
    SDL_Surface* imgMoon = IMG_Load("./Images/2k_moon.png");
    SDL_Surface* imgMercury = IMG_Load("./Images/2k_mercury.png");
    SDL_Surface* imgVenus = IMG_Load("./Images/2k_venus_surface.png");
    SDL_Surface* imgMars = IMG_Load("./Images/2k_mars.png");
    SDL_Surface* imgJupiter = IMG_Load("./Images/2k_jupiter.png");
    SDL_Surface* imgSaturne = IMG_Load("./Images/2k_saturn.png");
    //SDL_Surface* imgAnneauSaturne = IMG_Load("./Images/2k_saturn_ring_alpha.png");
    SDL_Surface* imgAnneauSaturne = IMG_Load("./Images/2k_saturn_ring_alpha_3.png");
    SDL_Surface* imgUranus = IMG_Load("./Images/2k_uranus.png");
    SDL_Surface* imgNeptune = IMG_Load("./Images/2k_neptune.png");
    SDL_Surface* imgEtoiles = IMG_Load("./Images/2k_stars_2.png");
    SDL_Surface* imgAsteroide = IMG_Load("./Images/2k_moon.png");
    SDL_Surface* imgFlammes = IMG_Load("./Images/2k_sun.png");



    //cretaion of texture
    GLuint textureSun;
    glGenTextures(1, &textureSun);
    GLuint textureEarth;
    glGenTextures(1, &textureEarth);
    GLuint textureMoon;
    glGenTextures(1, &textureMoon);
    GLuint textureMercury;
    glGenTextures(1, &textureMercury);
    GLuint textureVenus;
    glGenTextures(1, &textureVenus);
    GLuint textureMars;
    glGenTextures(1, &textureMars);
    GLuint textureJupiter;
    glGenTextures(1, &textureJupiter);
    GLuint textureSaturne;
    glGenTextures(1, &textureSaturne);
    GLuint textureAnneauSaturne;
    glGenTextures(1, &textureAnneauSaturne);
    GLuint textureUranus;
    glGenTextures(1, &textureUranus);
    GLuint textureNeptune;
    glGenTextures(1, &textureNeptune);
    GLuint textureEtoiles;
    glGenTextures(1, &textureEtoiles);
    GLuint textureAsteroide;
    glGenTextures(1, &textureAsteroide);
    GLuint textureFlammes;
    glGenTextures(1, &textureFlammes);


    createTexture(textureSun, imgSun);
    createTexture(textureEarth, imgEarth);
    createTexture(textureMoon, imgMoon);
    createTexture(textureMercury, imgMercury);
    createTexture(textureVenus, imgVenus);
    createTexture(textureMars, imgMars);
    createTexture(textureJupiter, imgJupiter);
    createTexture(textureSaturne, imgSaturne);
    createTexture(textureAnneauSaturne, imgAnneauSaturne);
    createTexture(textureUranus, imgUranus);
    createTexture(textureNeptune, imgNeptune);
    createTexture(textureEtoiles, imgEtoiles);
    createTexture(textureAsteroide, imgAsteroide);
    createTexture(textureFlammes, imgFlammes);



    //Initialisation of object
    Material sphereMtl{ {1.0f, 1.0f, 1.0f}, 0.4f, 0.9f, 0.8f, 100 };
    Material sphereMtlSun{ {1.0f, 1.0f, 1.0f}, 1.0, 0.5, 0.5f, 10 };

    Light light{ {0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f} };

    Sphere sphere(32, 32);

    //Generate and bind the VBO
    GLuint vboSphereID;
    glGenBuffers(1, &vboSphereID);
    glBindBuffer(GL_ARRAY_BUFFER, vboSphereID);
    //Set datas
    glBufferData(GL_ARRAY_BUFFER, sphere.getNbVertices() * (3 + 3 + 2) * sizeof(float), nullptr, GL_DYNAMIC_DRAW);   //3 coordinates per normal, 3 per position and 2 per UVs.
    glBufferSubData(GL_ARRAY_BUFFER, 0, sphere.getNbVertices() * 3 * sizeof(float), sphere.getVertices());
    glBufferSubData(GL_ARRAY_BUFFER, sphere.getNbVertices() * 3 * sizeof(float), sphere.getNbVertices() * 3 * sizeof(float), sphere.getNormals());
    glBufferSubData(GL_ARRAY_BUFFER, sphere.getNbVertices() * (3 + 3) * sizeof(float), sphere.getNbVertices() * 2 * sizeof(float), sphere.getUVs());
    glBindBuffer(GL_ARRAY_BUFFER, 0); //Close buffer

   //Create sun object of each planet, in order to have several operating speeds (invisible)
    GameObject sunGO;
    sunGO.vboID = vboSphereID;
    sunGO.geometry = &sphere;
    sunGO.texture = textureSun;

    GameObject sunGOEarth;
    sunGOEarth.vboID = vboSphereID;
    sunGOEarth.geometry = &sphere;
    sunGOEarth.texture = textureSun;

    GameObject sunGOMercury;
    sunGOMercury.vboID = vboSphereID;
    sunGOMercury.geometry = &sphere;

    GameObject sunGOVenus;
    sunGOVenus.vboID = vboSphereID;
    sunGOVenus.geometry = &sphere;

    GameObject sunGOMars;
    sunGOMars.vboID = vboSphereID;
    sunGOMars.geometry = &sphere;

    GameObject sunGOJupiter;
    sunGOJupiter.vboID = vboSphereID;
    sunGOJupiter.geometry = &sphere;

    GameObject sunGOSaturne;
    sunGOSaturne.vboID = vboSphereID;
    sunGOSaturne.geometry = &sphere;

    GameObject sunGOUranus;
    sunGOUranus.vboID = vboSphereID;
    sunGOUranus.geometry = &sphere;

    GameObject sunGONeptune;
    sunGONeptune.vboID = vboSphereID;
    sunGONeptune.geometry = &sphere;


    //Cretation of planet object
    GameObject earthGO;
    earthGO.vboID = vboSphereID;
    earthGO.geometry = &sphere;
    earthGO.texture = textureEarth;

    GameObject MoonGO;
    MoonGO.vboID = vboSphereID;
    MoonGO.geometry = &sphere;
    MoonGO.texture = textureMoon;

    GameObject Mercury;
    Mercury.vboID = vboSphereID;
    Mercury.geometry = &sphere;
    Mercury.texture = textureMercury;

    GameObject Venus;
    Venus.vboID = vboSphereID;
    Venus.geometry = &sphere;
    Venus.texture = textureVenus;

    GameObject Mars;
    Mars.vboID = vboSphereID;
    Mars.geometry = &sphere;
    Mars.texture = textureMars;

    GameObject Jupiter;
    Jupiter.vboID = vboSphereID;
    Jupiter.geometry = &sphere;
    Jupiter.texture = textureJupiter;

    GameObject Saturne;
    Saturne.vboID = vboSphereID;
    Saturne.geometry = &sphere;
    Saturne.texture = textureSaturne;

    GameObject anneauSaturne;
    anneauSaturne.vboID = vboSphereID;
    anneauSaturne.geometry = &sphere;
    anneauSaturne.texture = textureAnneauSaturne;

    GameObject Uranus;
    Uranus.vboID = vboSphereID;
    Uranus.geometry = &sphere;
    Uranus.texture = textureUranus;

    GameObject Neptune;
    Neptune.vboID = vboSphereID;
    Neptune.geometry = &sphere;
    Neptune.texture = textureNeptune;

    //background object
    GameObject Etoiles;
    Etoiles.vboID = vboSphereID;
    Etoiles.geometry = &sphere;
    Etoiles.texture = textureEtoiles;

    //about asteroide object
    GameObject sunGOAsteroide;
    sunGOAsteroide.vboID = vboSphereID;
    sunGOAsteroide.geometry = &sphere;

    GameObject Asteroide;
    Asteroide.vboID = vboSphereID;
    Asteroide.geometry = &sphere;
    Asteroide.texture = textureAsteroide;

    GameObject Flammes;
    Flammes.vboID = vboSphereID;
    Flammes.geometry = &sphere;
    Flammes.texture = textureFlammes;

    //initialisation of material parameter
    sunGO.sphereMtl = sphereMtlSun;
    earthGO.sphereMtl = sphereMtl;
    MoonGO.sphereMtl = sphereMtl;
    Mercury.sphereMtl = sphereMtl;
    Venus.sphereMtl = sphereMtl;
    Mars.sphereMtl = sphereMtl;
    Jupiter.sphereMtl = sphereMtl;
    Saturne.sphereMtl = sphereMtl;
    Uranus.sphereMtl = sphereMtl;
    Neptune.sphereMtl = sphereMtl;
    sunGOEarth.sphereMtl = sphereMtl;
    Etoiles.sphereMtl = sphereMtlSun;
    Asteroide.sphereMtl = sphereMtl;
    Flammes.sphereMtl = sphereMtl;


    //initialisation of light parameter
    sunGO.light = light;
    earthGO.light = light;
    MoonGO.light = light;
    Mercury.light = light;
    Venus.light = light;
    Mars.light = light;
    Jupiter.light = light;
    Saturne.light = light;
    Uranus.light = light;
    Neptune.light = light;
    sunGOEarth.light = light;
    Etoiles.light = light;
    anneauSaturne.light = light;
    Asteroide.light = light;
    Flammes.light = light;



    //Hierarchy of 3D objects (scene graph)
    sunGOEarth.children.push_back(&earthGO);
    earthGO.children.push_back(&MoonGO);

    sunGOMercury.children.push_back(&Mercury);
    sunGOVenus.children.push_back(&Venus);
    sunGOMars.children.push_back(&Mars);
    sunGOJupiter.children.push_back(&Jupiter);
    sunGOSaturne.children.push_back(&Saturne);
    sunGOSaturne.children.push_back(&anneauSaturne);
    sunGOUranus.children.push_back(&Uranus);
    sunGONeptune.children.push_back(&Neptune);
    sunGOAsteroide.children.push_back(&Asteroide);
    sunGOAsteroide.children.push_back(&Flammes);


    //Set variables for time (and operating speed)
    float tSun = 0;
    float tEarth = 0;
    float tMercury = 0;
    float tVenus = 0;
    float tMars = 0;
    float tJupiter = 0;
    float tSaturne = 0;
    float tUranus = 0;
    float tNeptune = 0;
    float tAsteroide = 0;
    float tEtoile = 0;


    //path of vertex and frangemnt shaders
    const char* vertexPath = "Shaders/colorTexture.vert";
    const char* fragPath = "Shaders/colorTexture.frag";

    //File shaders
    FILE* vertexFile = fopen(vertexPath, "r"); //"r" to read
    FILE* fragFile = fopen(fragPath, "r");

    //Load the files and Create shader
    Shader* shader = Shader::loadFromFiles(vertexFile, fragFile);
    fclose(vertexFile);
    fclose(fragFile);

    if (!shader) {
        std::cerr << "The shader is broken... from loading vertxFile and fragFile" << std::endl;
        return EXIT_FAILURE;
    }

    bool isOpened = true;

    //Main application loop
    while (isOpened) //affichage
    {
        //Time in ms telling us when this frame started. Useful for keeping a fix framerate
        uint32_t timeBegin = SDL_GetTicks();

        //Fetch the SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_CLOSE:
                    isOpened = false;
                    break;
                default:
                    break;
                }
                break;

            case SDL_KEYUP:
                isOpened = false;
                break;
                break;
                //We can add more event, like listening for the keyboard or the mouse. See SDL_Event documentation for more details
            }
        }

        //Clear the screen : the depth buffer and the color buffer
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);


        glm::mat4 view;
        view = glm::lookAt(glm::vec3(0.0, 2.0, 4.0), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 projection = glm::perspective(45.0f, WIDTH / (float)HEIGHT, 0.1f, 1000.0f);



        if (tAsteroide < 0 && tAsteroide > -14.1) {
            sunGO.localMatrix = sunGO.propagatedMatrix = glm::mat4(1.0f);
            sunGO.propagatedMatrix = glm::translate(sunGO.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
            sunGO.localMatrix = glm::scale(sunGO.localMatrix, glm::vec3(0.5f, 0.5f, 0.5f));
            sunGO.propagatedMatrix = glm::rotate(sunGO.propagatedMatrix, tMercury, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad
        }
        else if (tAsteroide < -14.35 && tAsteroide > -14.40) {
            sunGO.localMatrix = sunGO.propagatedMatrix = glm::mat4(1.0f);
            sunGO.propagatedMatrix = glm::translate(sunGO.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
            sunGO.localMatrix = glm::scale(sunGO.localMatrix, glm::vec3(0.55, 0.55, 0.55));
            sunGO.propagatedMatrix = glm::rotate(sunGO.propagatedMatrix, tMercury, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad
            //grow += 0.05f;
        }
        else if (tAsteroide < -14.40 && tAsteroide > -14.50) {
            sunGO.localMatrix = sunGO.propagatedMatrix = glm::mat4(1.0f);
            sunGO.propagatedMatrix = glm::translate(sunGO.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
            sunGO.localMatrix = glm::scale(sunGO.localMatrix, glm::vec3(0.60, 0.60, 0.60));
            sunGO.propagatedMatrix = glm::rotate(sunGO.propagatedMatrix, tMercury, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad
            //grow += 0.05f;
        }
        else if (tAsteroide < -14.50 && tAsteroide > -14.55) {
            sunGO.localMatrix = sunGO.propagatedMatrix = glm::mat4(1.0f);
            sunGO.propagatedMatrix = glm::translate(sunGO.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
            sunGO.localMatrix = glm::scale(sunGO.localMatrix, glm::vec3(0.65, 0.65, 0.65));
            sunGO.propagatedMatrix = glm::rotate(sunGO.propagatedMatrix, tMercury, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad
            //grow += 0.05f;
        }
        else if (tAsteroide < -14.55 && tAsteroide > -14.60) {
            sunGO.localMatrix = sunGO.propagatedMatrix = glm::mat4(1.0f);
            sunGO.propagatedMatrix = glm::translate(sunGO.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
            sunGO.localMatrix = glm::scale(sunGO.localMatrix, glm::vec3(0.70, 0.70, 0.70));
            sunGO.propagatedMatrix = glm::rotate(sunGO.propagatedMatrix, tMercury, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad
            //grow += 0.05f;
        }
        else if (tAsteroide < -14.60 && tAsteroide > -14.65) {
            sunGO.localMatrix = sunGO.propagatedMatrix = glm::mat4(1.0f);
            sunGO.propagatedMatrix = glm::translate(sunGO.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
            sunGO.localMatrix = glm::scale(sunGO.localMatrix, glm::vec3(0.75, 0.75, 0.75));
            sunGO.propagatedMatrix = glm::rotate(sunGO.propagatedMatrix, tMercury, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad
            sunGO.sphereMtl = { {0.2f, 0.2f, 0.2f}, 0.1, 0, 0.0f, 5 };
            //grow += 0.05f;
        }

        //Set Translation, Scaling and Rotation of each Object
        //Operation on fictional sun, result rotation of planet
        sunGOEarth.localMatrix = sunGOEarth.propagatedMatrix = glm::mat4(1.0f);
        sunGOEarth.propagatedMatrix = glm::translate(sunGOEarth.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        sunGOEarth.localMatrix = glm::scale(sunGOEarth.localMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
        sunGOEarth.propagatedMatrix = glm::rotate(sunGOEarth.propagatedMatrix, tEarth, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad

        sunGOMercury.localMatrix = sunGOMercury.propagatedMatrix = glm::mat4(1.0f);
        sunGOMercury.propagatedMatrix = glm::translate(sunGOMercury.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        sunGOMercury.localMatrix = glm::scale(sunGOEarth.localMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
        sunGOMercury.propagatedMatrix = glm::rotate(sunGOMercury.propagatedMatrix, tMercury, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad

        sunGOVenus.localMatrix = sunGOVenus.propagatedMatrix = glm::mat4(1.0f);
        sunGOVenus.propagatedMatrix = glm::translate(sunGOVenus.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        sunGOVenus.localMatrix = glm::scale(sunGOVenus.localMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
        sunGOVenus.propagatedMatrix = glm::rotate(sunGOVenus.propagatedMatrix, tVenus, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad

        sunGOMars.localMatrix = sunGOMars.propagatedMatrix = glm::mat4(1.0f);
        sunGOMars.propagatedMatrix = glm::translate(sunGOMars.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        sunGOMars.localMatrix = glm::scale(sunGOMars.localMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
        sunGOMars.propagatedMatrix = glm::rotate(sunGOMars.propagatedMatrix, tMars, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad

        sunGOJupiter.localMatrix = sunGOJupiter.propagatedMatrix = glm::mat4(1.0f);
        sunGOJupiter.propagatedMatrix = glm::translate(sunGOJupiter.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        sunGOJupiter.localMatrix = glm::scale(sunGOJupiter.localMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
        sunGOJupiter.propagatedMatrix = glm::rotate(sunGOJupiter.propagatedMatrix, tJupiter, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad

        sunGOSaturne.localMatrix = sunGOSaturne.propagatedMatrix = glm::mat4(1.0f);
        sunGOSaturne.propagatedMatrix = glm::translate(sunGOSaturne.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        sunGOSaturne.localMatrix = glm::scale(sunGOSaturne.localMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
        sunGOSaturne.propagatedMatrix = glm::rotate(sunGOSaturne.propagatedMatrix, tSaturne, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad

        sunGOUranus.localMatrix = sunGOUranus.propagatedMatrix = glm::mat4(1.0f);
        sunGOUranus.propagatedMatrix = glm::translate(sunGOUranus.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        sunGOUranus.localMatrix = glm::scale(sunGOUranus.localMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
        sunGOUranus.propagatedMatrix = glm::rotate(sunGOUranus.propagatedMatrix, tUranus, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad

        sunGONeptune.localMatrix = sunGONeptune.propagatedMatrix = glm::mat4(1.0f);
        sunGONeptune.propagatedMatrix = glm::translate(sunGONeptune.localMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        sunGONeptune.localMatrix = glm::scale(sunGONeptune.localMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
        sunGONeptune.propagatedMatrix = glm::rotate(sunGONeptune.propagatedMatrix, tNeptune, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad


        //Operation on planet
        Mercury.localMatrix = Mercury.propagatedMatrix = glm::mat4(1.0);
        Mercury.propagatedMatrix = glm::translate(Mercury.propagatedMatrix, glm::vec3(0.55f, 0.0f, 0.0f));
        Mercury.localMatrix = glm::scale(Mercury.localMatrix, glm::vec3(0.12f, 0.12f, 0.12f));
        Mercury.propagatedMatrix = glm::rotate(Mercury.propagatedMatrix, tMercury, glm::vec3(0.0f, 1.0f, 1.0f));

        Venus.localMatrix = Venus.propagatedMatrix = glm::mat4(1.0);
        Venus.propagatedMatrix = glm::translate(Venus.propagatedMatrix, glm::vec3(0.75f, 0.0f, 0.0f));
        Venus.localMatrix = glm::scale(Venus.localMatrix, glm::vec3(0.12f, 0.12f, 0.12f));
        Venus.propagatedMatrix = glm::rotate(Venus.propagatedMatrix, tVenus, glm::vec3(0.0f, 1.0f, 1.0f));

        earthGO.localMatrix = earthGO.propagatedMatrix = glm::mat4(1.0f);
        earthGO.localMatrix = glm::scale(earthGO.localMatrix, glm::vec3(0.12f, 0.12f, 0.12f));
        earthGO.propagatedMatrix = glm::translate(earthGO.propagatedMatrix, glm::vec3(0.85f, 0.f, 0.0f));
        earthGO.propagatedMatrix = glm::rotate(earthGO.propagatedMatrix, tMercury, glm::vec3(1.0f, 0.0f, 0.0f)); //tMercury for faster rotation

        MoonGO.localMatrix = MoonGO.propagatedMatrix = glm::mat4(1.0f);
        MoonGO.localMatrix = glm::scale(MoonGO.localMatrix, glm::vec3(0.04f, 0.04f, 0.04f));
        MoonGO.propagatedMatrix = glm::translate(MoonGO.propagatedMatrix, glm::vec3(0.0f, 0.10f, 0.0f));
        MoonGO.propagatedMatrix = glm::rotate(MoonGO.propagatedMatrix, tSun, glm::vec3(0.0f, 1.0f, 0.0f));

        Mars.localMatrix = Mars.propagatedMatrix = glm::mat4(1.0);
        Mars.propagatedMatrix = glm::translate(Mars.propagatedMatrix, glm::vec3(0.95f, 0.0f, 0.0f));
        Mars.localMatrix = glm::scale(Mars.localMatrix, glm::vec3(0.12f, 0.12f, 0.12f));
        Mars.propagatedMatrix = glm::rotate(Mars.propagatedMatrix, tEarth, glm::vec3(0.0f, 1.0f, 1.0f));

        Jupiter.localMatrix = Jupiter.propagatedMatrix = glm::mat4(1.0);
        Jupiter.propagatedMatrix = glm::translate(Jupiter.propagatedMatrix, glm::vec3(1.30f, 0.0f, 0.0f));
        Jupiter.localMatrix = glm::scale(Jupiter.localMatrix, glm::vec3(0.30f, 0.30f, 0.30f));
        Jupiter.propagatedMatrix = glm::rotate(Jupiter.propagatedMatrix, tEarth, glm::vec3(0.2f, 1.0f, 0.0f));

        Saturne.localMatrix = Saturne.propagatedMatrix = glm::mat4(1.0);
        Saturne.propagatedMatrix = glm::translate(Saturne.propagatedMatrix, glm::vec3(1.90f, 0.0f, 0.0f));
        Saturne.localMatrix = glm::scale(Saturne.localMatrix, glm::vec3(0.20f, 0.20f, 0.20f));
        Saturne.propagatedMatrix = glm::rotate(Saturne.propagatedMatrix, tEarth, glm::vec3(0.0f, 1.0f, 0.2f));

        anneauSaturne.localMatrix = anneauSaturne.propagatedMatrix = glm::mat4(1.0);
        anneauSaturne.propagatedMatrix = glm::translate(anneauSaturne.propagatedMatrix, glm::vec3(1.90f, 0.0f, 0.0f));
        anneauSaturne.localMatrix = glm::scale(anneauSaturne.localMatrix, glm::vec3(0.35f, 0.015f, 0.35f));
        anneauSaturne.propagatedMatrix = glm::rotate(anneauSaturne.propagatedMatrix, tEarth, glm::vec3(0.0f, 1.0f, 0.2f));

        Uranus.localMatrix = Uranus.propagatedMatrix = glm::mat4(1.0);
        Uranus.propagatedMatrix = glm::translate(Uranus.propagatedMatrix, glm::vec3(2.5f, 0.0f, 0.0f));
        Uranus.localMatrix = glm::scale(Uranus.localMatrix, glm::vec3(0.12f, 0.12f, 0.12f));
        Uranus.propagatedMatrix = glm::rotate(Uranus.propagatedMatrix, tEarth, glm::vec3(0.0f, 1.0f, 1.0f));

        Neptune.localMatrix = Neptune.propagatedMatrix = glm::mat4(1.0);
        Neptune.propagatedMatrix = glm::translate(Neptune.propagatedMatrix, glm::vec3(2.9f, 0.0f, 0.0f));
        Neptune.localMatrix = glm::scale(Neptune.localMatrix, glm::vec3(0.12f, 0.12f, 0.12f));
        Neptune.propagatedMatrix = glm::rotate(Neptune.propagatedMatrix, tEarth, glm::vec3(0.0f, 1.0f, 1.0f));

        Etoiles.localMatrix = Etoiles.propagatedMatrix = glm::mat4(1.0);
        Etoiles.propagatedMatrix = glm::translate(Etoiles.propagatedMatrix, glm::vec3(0.0f, 0.0f, 2.0f));
        Etoiles.localMatrix = glm::scale(Etoiles.localMatrix, glm::vec3(15.0f, 15.0f, 15.0f));
        Etoiles.propagatedMatrix = glm::rotate(Etoiles.propagatedMatrix, tEtoile, glm::vec3(1.0f, 1.0f, 1.0f));


        //definition of the asteroid's motion
        if (tAsteroide < 0 && tAsteroide > -12) {
            sunGOAsteroide.localMatrix = sunGOAsteroide.propagatedMatrix = glm::mat4(1.0f);
            sunGOAsteroide.propagatedMatrix = glm::translate(sunGOAsteroide.propagatedMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
            sunGOAsteroide.localMatrix = glm::scale(sunGOAsteroide.localMatrix, glm::vec3(0.1f, 0.1f, 0.1f));
            sunGOAsteroide.propagatedMatrix = glm::rotate(sunGOAsteroide.propagatedMatrix, tAsteroide, glm::vec3(0.0f, 1.0f, 0.0f)); //angle de rotation, en rad
            tAsteroide -= 0.01;
        }
        else {
            sunGOAsteroide.localMatrix = sunGOAsteroide.propagatedMatrix = glm::mat4(1.0f);
            sunGOAsteroide.propagatedMatrix = glm::translate(sunGOAsteroide.propagatedMatrix, glm::vec3(1.42f, -1.45f, 0.0f));
            sunGOAsteroide.localMatrix = glm::scale(sunGOAsteroide.localMatrix, glm::vec3(0.0001f, 0.0001f, 0.0001f));
            sunGOAsteroide.propagatedMatrix = glm::rotate(sunGOAsteroide.propagatedMatrix, tAsteroide, glm::vec3(1.0f, 1.0f, 0.0f)); //angle de rotation, en rad
            tAsteroide -= 0.005;
        }

        if (tAsteroide < 0 && tAsteroide > -5) {
            Asteroide.localMatrix = Asteroide.propagatedMatrix = glm::mat4(1.0);
            Asteroide.propagatedMatrix = glm::translate(Asteroide.propagatedMatrix, glm::vec3(1.5f, 1.5f, 5.0f));
            Asteroide.localMatrix = glm::scale(Asteroide.localMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
            Flammes.localMatrix = Flammes.propagatedMatrix = glm::mat4(1.0);
            Flammes.propagatedMatrix = glm::translate(Flammes.propagatedMatrix, glm::vec3(0.0f, 0.0f, 150.0f));
            Flammes.localMatrix = glm::scale(Flammes.localMatrix, glm::vec3(0.3f, 0.28f, 0.3f));
        }
        else if (tAsteroide < -5 && tAsteroide > -12) {
            Asteroide.localMatrix = Asteroide.propagatedMatrix = glm::mat4(1.0);
            Asteroide.propagatedMatrix = glm::translate(Asteroide.propagatedMatrix, glm::vec3(1.0f, 1.0f, 2.7f));
            Asteroide.localMatrix = glm::scale(Asteroide.localMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
            Flammes.localMatrix = Flammes.propagatedMatrix = glm::mat4(1.0);
            Flammes.propagatedMatrix = glm::translate(Flammes.propagatedMatrix, glm::vec3(0.0f, 0.0f, 150.0f));
            Flammes.localMatrix = glm::scale(Flammes.localMatrix, glm::vec3(0.3f, 0.28f, 0.3f));
        }
        else if (tAsteroide < -12 && tAsteroide > -14.1) {
            Asteroide.localMatrix = Asteroide.propagatedMatrix = glm::mat4(1.0);
            Asteroide.propagatedMatrix = glm::translate(Asteroide.propagatedMatrix, glm::vec3(0.0f, 0.0f, 2.0f));
            Asteroide.localMatrix = glm::scale(Asteroide.localMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
            Flammes.localMatrix = Flammes.propagatedMatrix = glm::mat4(1.0);
            Flammes.propagatedMatrix = glm::translate(Flammes.propagatedMatrix, glm::vec3(0.16f, 0.0f, 2.0f));
            Flammes.localMatrix = glm::scale(Flammes.localMatrix, glm::vec3(0.6, 0.3f, 0.3f));

        }
        else {
            Asteroide.localMatrix = Asteroide.propagatedMatrix = glm::mat4(1.0);
            Asteroide.propagatedMatrix = glm::translate(Asteroide.propagatedMatrix, glm::vec3(0.0f, 0.0f, 150.0f));
            Asteroide.localMatrix = glm::scale(Asteroide.localMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
            Flammes.localMatrix = Flammes.propagatedMatrix = glm::mat4(1.0);
            Flammes.propagatedMatrix = glm::translate(Flammes.propagatedMatrix, glm::vec3(0.0f, 0.0f, 150.0f));
            Flammes.localMatrix = glm::scale(Flammes.localMatrix, glm::vec3(0.3f, 0.28f, 0.3f));
        }


        //Change time at each loop
        tSun += 0.007f;
        tMercury += 0.01f;
        tVenus += 0.009f;
        tEarth += 0.0077f;
        tMars += 0.0064f;
        tJupiter += 0.004f;
        tSaturne += 0.003f;
        tUranus += 0.002f;
        tNeptune += 0.001f;
        tEtoile += 0.0002f;

        //Set Vision
        std::stack<glm::mat4> matrices;
        matrices.push(projection * view);


        //Draw planet on screen, with light
        if (tAsteroide < -16.0) {
            break;
        }
        else if (tAsteroide < -15.4) {
            tAsteroide -= 0.01;
        }
        else if (tAsteroide < -15.0) {
            drawSphere(Etoiles, shader, matrices);
            tAsteroide -= 0.01;
        }
        else if (tAsteroide < -14.60) {
            drawSphere(Etoiles, shader, matrices);
            drawSphere(sunGO, shader, matrices);
            tAsteroide -= 0.01;
        }
        else {
            drawSphere(sunGO, shader, matrices);
            drawSphere(sunGOEarth, shader, matrices);
            drawSphere(sunGOMercury, shader, matrices);
            drawSphere(sunGOVenus, shader, matrices);
            drawSphere(sunGOMars, shader, matrices);
            drawSphere(sunGOJupiter, shader, matrices);
            drawSphere(sunGOSaturne, shader, matrices);
            drawSphere(sunGOUranus, shader, matrices);
            drawSphere(sunGONeptune, shader, matrices);
            drawSphere(Etoiles, shader, matrices);
            drawSphere(sunGOAsteroide, shader, matrices);
        }



        //Display on screen (swap the buffer on screen and the buffer you are drawing on)
        SDL_GL_SwapWindow(window);

        //Time in ms telling us when this frame ended. Useful for keeping a fix framerate
        uint32_t timeEnd = SDL_GetTicks();

        //We want FRAMERATE FPS
        if (timeEnd - timeBegin < TIME_PER_FRAME_MS)
            SDL_Delay((uint32_t)(TIME_PER_FRAME_MS)-(timeEnd - timeBegin));
    }

    //Delete Buffer and Shader
    glDeleteBuffers(1, &vboSphereID);
    delete shader;

    //Free everything
    if (context != NULL)
        SDL_GL_DeleteContext(context);
    if (window != NULL)
        SDL_DestroyWindow(window);

    //THE_END//

    return 0;
}