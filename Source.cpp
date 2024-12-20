#include <iostream>
#include <string>
#include <assert.h>

#include <vector>
#include <fstream>
#include <sstream>
#include <map>

#include <fstream>
#include <json.hpp>

using json = nlohmann::json;
using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"

//STB_IMAGE
#include <stb_image.h>

struct Material
{
	string name;
	float ns;
	glm::vec3 ka;
	glm::vec3 kd;
	glm::vec3 ks;
	glm::vec3 ke;
	float ni;
	float d;
	int illum;
};

struct Mesh 
{
	GLuint VAO; 
	int nVertices;
	glm::mat4 model;
	Material material;
	GLuint texID;
};

struct Object
{
	vector<Mesh> meshes;
	float fatorEscala = 0.0f;
	float translacaoX = 0.0f;
	float translacaoY = 0.0f; 
	float translacaoZ = 0.0f;
	bool aplicarCurva = false;
};

// Propriedades parametrizáveis que são buscadas do arquivo de configuração:
float ka, ks, kd, ns;
glm::vec3 ambientColor, specularColor, diffuseColor, lightPos;
glm::vec3 cameraPos, cameraFront, cameraUp;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

vector<Mesh> loadSimpleOBJ(string filePATH);
int loadSimplePLY(string filePath, int &nVertices);
GLuint configureVertexAndBuffer(vector <GLfloat> vBuffer, int &nVertices);
GLuint loadTexture(string filePath, int &width, int &height);
void loadConfiguration(const std::string& configPath);

// Dimensões da janela (pode ser alterado em tempo de execução)
const GLuint WIDTH = 2000, HEIGHT = 1000;

bool rotateX=false, rotateY=false, rotateZ=false;

vector<Object> objetos;
int objSelecionado = 0; // Variável para armazenar o índice do objeto selecionado (0, 1 ou 2)
float reset = 0.0f;

// Função que representa minha curva paramétrica para uma trajetória circular
glm::mat4 calculateHurricaneMotion(glm::mat4 model, float time) {
    // Parâmetros da espiral
    float angularSpeed = glm::radians(45.0f);  // Velocidade de rotação no eixo Y
    float radius = 0.1f + 0.2f * time;         // Raio crescendo lentamente
    float height = -3.0f + 0.2f * time;        // Altura aumentando com o tempo
    float speed = 1.0f;                        // Velocidade da espiral

    // Evitar raio negativo
    radius = glm::max(radius, 0.1f);

    // Calcular posição na espiral
    float angle = time * speed;                // Ângulo da espiral
    float x = radius * cos(angle);
    float z = radius * sin(angle);
    float y = height;

    // Criar a matriz de transformação
    model = glm::mat4();

    // Translação para posição na espiral
    model = glm::translate(model, glm::vec3(x, y, z));

    // Rotação contínua no eixo Y
    model = glm::rotate(model, time * angularSpeed, glm::vec3(0.0f, 1.0f, 0.0f));

    return model;
}


const GLchar* vertexShaderSource = "#version 430\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"layout (location = 2) in vec2 texc;\n"
"layout (location = 3) in vec3 normal;\n"
"\n"
"uniform mat4 model;\n"
"uniform mat4 projection;\n"
"uniform mat4 view;\n"
"\n"
"//Variáveis que irão para o fragment shader\n"
"out vec3 finalColor;\n"
"out vec2 texCoord;\n"
"out vec3 scaledNormal;\n"
"out vec3 fragPos;\n"
"\n"
"void main()\n"
"{\n"
"	//...pode ter mais linhas de código aqui!\n"
"	gl_Position = projection * view * model * vec4(position, 1.0);\n"
"	finalColor = color;\n"
"    texCoord = vec2(texc.s, 1 - texc.t);\n"
"    fragPos = vec3(model * vec4(position, 1.0));\n"
"    scaledNormal = vec3(model * vec4(normal, 1.0));\n"
"}\n\0";

//Códifo fonte do Fragment Shader (em GLSL): ainda hardcoded
const GLchar* fragmentShaderSource = "#version 430\n"
"\n"
"in vec3 finalColor;\n"
"in vec2 texCoord;\n"
"in vec3 scaledNormal;\n"
"in vec3 fragPos;\n"
"\n"
"uniform float ka, kd, ks, ns;\n"
"\n"
"uniform vec3 lightPos, ambientColor, diffuseColor, specularColor;\n"
"\n"
"uniform vec3 cameraPos;\n"
"\n"
"out vec4 color;\n"
"uniform sampler2D texBuffer;\n"
"\n"
"void main()\n"
"{\n"
"    vec3 ambient = ka * ambientColor;\n"
"\n"
"    vec3 diffuse;\n"
"    vec3 N = normalize(scaledNormal);\n"
"    vec3 L = normalize(lightPos - fragPos);\n"
"    float diff = max(dot(N,L),0.0);\n"
"    diffuse = kd * diff * diffuseColor;\n"
"\n"
"    vec3 specular;\n"
"    vec3 R = normalize(reflect(-L,N));\n"
"    vec3 V = normalize(cameraPos - fragPos);\n"
"    float spec = max(dot(R,V),0.0);\n"
"    spec = pow(spec,ns);\n"
"    specular = ks * spec * specularColor;\n"
"\n"
"  	 vec4 texColor = texture(texBuffer,texCoord);\n"
"    vec3 result = (ambient + diffuse) * vec3(texColor) + specular;\n"
"\n"
"    color = vec4(result,1.0);\n"
"}\n\0";


// Função MAIN
int main()
{
	glfwInit();

	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Visualizador 3D", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Fazendo o registro da função de callback para a janela GLFW
	glfwSetKeyCallback(window, key_callback);

	// GLAD: carrega todos os ponteiros d funções da OpenGL
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
	}

	// Obtendo as informações de versão
	const GLubyte* renderer = glGetString(GL_RENDERER); /* get renderer string */
	const GLubyte* version = glGetString(GL_VERSION); /* version as a string */
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported " << version << endl;

	// Definindo as dimensões da viewport com as mesmas dimensões da janela da aplicação
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	// Carrega as configurações
	// loadConfiguration("config.json");
	loadConfiguration("C:\\Users\\Patrick\\Desktop\\pg\\2024\\trabGB\\Visualizador3D\\config.json");

	// Shader shader("phong.vs","phong.fs");
	Shader shader(vertexShaderSource, fragmentShaderSource, false);
	glUseProgram(shader.ID);

	//Matriz de modelo
	glm::mat4 model = glm::mat4(1); 
	GLint modelLoc = glGetUniformLocation(shader.ID, "model");
	model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	//Matriz de view
	glm::mat4 view = glm::lookAt(cameraPos,glm::vec3(0.0f,0.0f,0.0f),cameraUp);
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	
	//Matriz de projeção
	glm::mat4 projection = glm::perspective(glm::radians(39.6f),(float)WIDTH/HEIGHT,0.1f,100.0f);
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	glEnable(GL_DEPTH_TEST);

	shader.setFloat("ka", ka);
	shader.setFloat("ks", ks);
	shader.setFloat("kd", kd);
	shader.setVec3("lightPos", lightPos.x, lightPos.y, lightPos.z);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glLineWidth(10);
		glPointSize(20);

		float angle = (GLfloat)glfwGetTime();

		float time = (float)glfwGetTime(); // Get elapsed time

		for (int i = 0; i < objetos.size(); i++) {
			for (int m = 0; m < objetos[i].meshes.size(); m++) {
				if (objetos[i].meshes[m].material.name.empty()) {
					shader.setVec3("ambientColor", ambientColor.r, ambientColor.g, ambientColor.b);
					shader.setVec3("specularColor", specularColor.r, specularColor.g, specularColor.b);
					shader.setVec3("diffuseColor", diffuseColor.r, diffuseColor.g, diffuseColor.b);
					shader.setFloat("ns", ns);
				}
				else {
					shader.setVec3("ambientColor", objetos[i].meshes[m].material.ka.x, objetos[i].meshes[m].material.ka.y, objetos[i].meshes[m].material.ka.z);
					shader.setVec3("specularColor", objetos[i].meshes[m].material.ks.x, objetos[i].meshes[m].material.ks.y, objetos[i].meshes[m].material.ks.z);
					shader.setVec3("diffuseColor", objetos[i].meshes[m].material.kd.x, objetos[i].meshes[m].material.kd.y, objetos[i].meshes[m].material.kd.z);
					shader.setFloat("ns", objetos[i].meshes[m].material.ns);
				}

				// Aplicar transformações apenas ao cubo selecionado
				objetos[objSelecionado].meshes[m].model = glm::mat4(); // Resetar a matriz

				// Aplicar Curva Paramétrica sempre ao objeto zero
				if (objetos[i].aplicarCurva) {
					objetos[i].meshes[m].model = calculateHurricaneMotion(objetos[i].meshes[m].model, time - reset);
				}

				// Aplique as rotações conforme o eixo selecionado
				if (rotateX)
				{
					objetos[objSelecionado].meshes[m].model = glm::rotate(objetos[objSelecionado].meshes[m].model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
				}
				else if (rotateY)
				{
					objetos[objSelecionado].meshes[m].model = glm::rotate(objetos[objSelecionado].meshes[m].model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
				}
				else if (rotateZ)
				{
					objetos[objSelecionado].meshes[m].model = glm::rotate(objetos[objSelecionado].meshes[m].model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
				}

				// Aplicar a escala
				objetos[objSelecionado].meshes[m].model = glm::scale(objetos[objSelecionado].meshes[m].model, glm::vec3(objetos[objSelecionado].fatorEscala, objetos[objSelecionado].fatorEscala, objetos[objSelecionado].fatorEscala));

				// Aplicar a translação
				objetos[objSelecionado].meshes[m].model = glm::translate(objetos[objSelecionado].meshes[m].model, glm::vec3(objetos[objSelecionado].translacaoX, objetos[objSelecionado].translacaoY, objetos[objSelecionado].translacaoZ));
				

				// Passe a matriz 'model' para o shader para cada cubo
				glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(objetos[i].meshes[m].model));
				
				// Atualizar a view (câmera)
				glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
				glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
				
				shader.setVec3("cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);
			
				// Renderizar todos os cubos
				glBindVertexArray(objetos[i].meshes[m].VAO);
				glBindTexture(GL_TEXTURE_2D, objetos[i].meshes[m].texID);
				glDrawArrays(GL_TRIANGLES, 0, objetos[i].meshes[m].nVertices);
			}
		}

		glfwSwapBuffers(window);
	}

	// glDeleteVertexArrays(1, &objSelecionado.VAO);
	for (int i = 0; i < objetos.size(); i++) {
		for (int m = 0; m < objetos[i].meshes.size(); m++) {
			glDeleteVertexArrays(1, &objetos[i].meshes[m].VAO);
		}
	}
	glfwTerminate();
	return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	if (key == GLFW_KEY_X && action == GLFW_PRESS)
	{
		rotateX = true;
		rotateY = false;
		rotateZ = false;
	}

	if (key == GLFW_KEY_Y && action == GLFW_PRESS)
	{
		rotateX = false;
		rotateY = true;
		rotateZ = false;
	}

	if (key == GLFW_KEY_Z && action == GLFW_PRESS)
	{
		rotateX = false;
		rotateY = false;
		rotateZ = true;
	}

	if (key == GLFW_KEY_1 && action == GLFW_PRESS)
		objSelecionado = 0;
	if (key == GLFW_KEY_2 && action == GLFW_PRESS)
		objSelecionado = 1;
	if (key == GLFW_KEY_3 && action == GLFW_PRESS)
		objSelecionado = 2;

	// Reseta a animação do furacão
	if (key == GLFW_KEY_0)
		reset = (float)glfwGetTime();

	//Verifica a movimentação da câmera
	float cameraSpeed = 0.05f;

	if (action == GLFW_REPEAT){
		if ((key == GLFW_KEY_W || key == GLFW_KEY_UP))
		{
			cameraPos += cameraSpeed * cameraFront;
		}
		if ((key == GLFW_KEY_S || key == GLFW_KEY_DOWN))
		{
			cameraPos -= cameraSpeed * cameraFront;
		}
		if ((key == GLFW_KEY_A || key == GLFW_KEY_LEFT))
		{
			cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
		}
		if ((key == GLFW_KEY_D || key == GLFW_KEY_RIGHT))
		{
			cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
		}
		if (key == GLFW_KEY_Q)
		{
			objetos[objSelecionado].fatorEscala += 0.1f;
		}
		if (key == GLFW_KEY_E)
		{
			objetos[objSelecionado].fatorEscala -= 0.1f;
		}
		if (key == GLFW_KEY_4)
		{
			objetos[objSelecionado].translacaoX += 0.1f;
		}
		if (key == GLFW_KEY_5)
		{
			objetos[objSelecionado].translacaoX -= 0.1f;
		}
		if (key == GLFW_KEY_6)
		{
			objetos[objSelecionado].translacaoY += 0.1f;
		}
		if (key == GLFW_KEY_7)
		{
			objetos[objSelecionado].translacaoY -= 0.1f;
		}
		if (key == GLFW_KEY_8)
		{
			objetos[objSelecionado].translacaoZ += 0.1f;
		}
		if (key == GLFW_KEY_9)
		{
			objetos[objSelecionado].translacaoZ -= 0.1f;
		}
	}
}


map<string, Material> loadMtlLib(string mtllib) {
	string currentName;
	map<string, Material> materials;
	ifstream arqEntrada;
	arqEntrada.open(mtllib.c_str());
	if (arqEntrada.is_open())
	{
		string line;
		while (getline(arqEntrada, line))
		{
			istringstream ssline(line);
			string word;
			ssline >> word;
			if (word == "newmtl") 
			{
				Material mat;
				ssline >> currentName;
				mat.name = currentName;
				materials[currentName] = mat;
			}
			else if (word == "Ns")
			{
				string ns;
				ssline >> ns;
				materials[currentName].ns = stof(ns);
			} 
			else if (word == "Ka")
			{
				glm::vec3 ka;
				ssline >> ka.x >> ka.y >> ka.z;
				materials[currentName].ka = ka;
			} 
			else if (word == "Kd")
			{
				glm::vec3 kd;
				ssline >> kd.x >> kd.y >> kd.z;
				materials[currentName].kd = kd;
			} 
			else if (word == "Ks")
			{
				glm::vec3 ks;
				ssline >> ks.x >> ks.y >> ks.z;
				materials[currentName].ks = ks;
			} 
			else if (word == "Ke")
			{
				glm::vec3 ke;
				ssline >> ke.x >> ke.y >> ke.z;
				materials[currentName].ke = ke;
			} 
			else if (word == "Ni")
			{
				string ni;
				ssline >> ni;
				materials[currentName].ni = stof(ni);
			} 
			else if (word == "d")
			{
				string d;
				ssline >> d;
				materials[currentName].d = stof(d);
			} 
			else if (word == "illum")
			{
				string illum;
				ssline >> illum;
				materials[currentName].illum = stoi(illum);
			} 
		}
	}
	else
	{
		cout << "Erro ao tentar ler o arquivo " << mtllib << endl;
	}
	return materials;
}

vector<Mesh> loadSimpleOBJ(string objFilePath, string textureFilePath)
{
	string currentName;
	map<string, Material> materials;
	vector<Mesh> meshes;

	vector <glm::vec3> vertices;
	vector <glm::vec2> texCoords;
	vector <glm::vec3> normals;
	vector <GLfloat> vBuffer;
	int nVertices;

	glm::vec3 color = glm::vec3(1.0, 1.0, 1.0);

	ifstream arqEntrada;

	arqEntrada.open(objFilePath.c_str());
	if (arqEntrada.is_open())
	{
		string line;
		while (getline(arqEntrada, line))
		{
			istringstream ssline(line);
			string word;
			ssline >> word;
			if (word == "o") 
			{
				if (!vBuffer.empty()) {
					GLuint vao = configureVertexAndBuffer(vBuffer, nVertices);
					int texWidth, texHeight;
					Mesh mesh = {
						.VAO = vao,
						.nVertices = nVertices,
						.model = glm::mat4(1),
						.material = materials[currentName],
						.texID = loadTexture(textureFilePath, texWidth, texHeight), 
					};

					meshes.push_back(mesh);
					vBuffer.clear();
				}
			}
			else if (word == "g")
			{
				vBuffer.clear();
			}
			else if (word == "mtllib") {
				string mtllib;
				ssline >> mtllib;
				materials = loadMtlLib(mtllib);
			}
			else if (word == "usemtl") {
				ssline >> currentName;
			}
			else if (word == "v")
			{
				glm::vec3 vertice;
				ssline >> vertice.x >> vertice.y >> vertice.z;
				vertices.push_back(vertice);
			}
			else if (word == "vt")
			{
				glm::vec2 vt;
				ssline >> vt.s >> vt.t;
				texCoords.push_back(vt);
			}
			else if (word == "vn")
			{
				glm::vec3 normal;
				ssline >> normal.x >> normal.y >> normal.z;
				normals.push_back(normal);
			}
			else if (word == "f")
			{
				while (ssline >> word) 
				{
					int vi, ti, ni;
					istringstream ss(word);
    				std::string index;

    				std::getline(ss, index, '/');
    				vi = std::stoi(index) - 1; 
    				std::getline(ss, index, '/');
    				ti = std::stoi(index) - 1;
    				std::getline(ss, index);
    				ni = std::stoi(index) - 1;

					vBuffer.push_back(vertices[vi].x);
					vBuffer.push_back(vertices[vi].y);
					vBuffer.push_back(vertices[vi].z);
					
					vBuffer.push_back(color.r);
					vBuffer.push_back(color.g);
					vBuffer.push_back(color.b);

					vBuffer.push_back(texCoords[ti].s);
					vBuffer.push_back(texCoords[ti].t);

					vBuffer.push_back(normals[ni].x);
					vBuffer.push_back(normals[ni].y);
					vBuffer.push_back(normals[ni].z);
    			}
			}
		}

		arqEntrada.close();

		GLuint vao = configureVertexAndBuffer(vBuffer, nVertices);
		Mesh mesh = {
			.VAO = vao,
			.nVertices = nVertices,
			.model = glm::mat4(1),
			.material = materials[currentName]
		};
		meshes.push_back(mesh);
		vBuffer.clear();
	}
	else
	{
		cout << "Erro ao tentar ler o arquivo " << objFilePath << endl;
	}

	return meshes;
}

GLuint configureVertexAndBuffer(vector <GLfloat> vBuffer, int &nVertices) {
	GLuint VBO, VAO;

	glGenBuffers(1, &VBO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	glBufferData(GL_ARRAY_BUFFER, vBuffer.size()* sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);		

	glGenVertexArrays(1, &VAO);

	glBindVertexArray(VAO);
	
	//Atributo posição (x, y, z)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	//Atributo cor (r, g, b)
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(3*sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	//Atributo coordenada de textura - s, t
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(6*sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	//Atributo vetor normal - x, y, z
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(8*sizeof(GLfloat)));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
	nVertices = vBuffer.size() / 2;

	return VAO;
}

int loadSimplePLY(string filePath, int &nVertices)
{
    GLuint VBO, VAO;
    vector<glm::vec3> vertices;
    vector<GLfloat> vBuffer;

    ifstream arqEntrada(filePath.c_str());
    if (arqEntrada)
    {
        string line;

        // lê até encontrar o cabeçalho
        while (getline(arqEntrada, line))
        {
            if (line == "end_header") 
                break;
        }

        // lê os vértices
        for (int i = 0; i < 8; i++)
        {
            getline(arqEntrada, line);
            istringstream ssline(line);
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        }

        // lê as faces
        for (int i = 0; i < 12; i++)
        {
            getline(arqEntrada, line);
            istringstream ssline(line);
            int n, idx[3];  // cada face tem 3 vértices (triângulos)
            ssline >> n >> idx[0] >> idx[1] >> idx[2]; 

            for (int j = 0; j < 3; j++) {
                glm::vec3 vec = vertices.at(idx[j]);
                // coordenadas
                vBuffer.push_back(vec.x);
                vBuffer.push_back(vec.y);
                vBuffer.push_back(vec.z);
                // cor
                vBuffer.push_back(0.0f);  // cor R
                vBuffer.push_back(0.0f);  // cor G
                vBuffer.push_back(1.0f);  // cor B
                nVertices++;
            }
        }

        // Geração e configuração do VBO e VAO
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

		glGenVertexArrays(1, &VAO);
		glGenVertexArrays(1, &VAO);

		// Vincula (bind) o VAO primeiro, e em seguida  conecta e seta o(s) buffer(s) de vértices
		// e os ponteiros para os atributos 
        glGenVertexArrays(1, &VAO);

		// Vincula (bind) o VAO primeiro, e em seguida  conecta e seta o(s) buffer(s) de vértices
		// e os ponteiros para os atributos 
        glBindVertexArray(VAO);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(0);
        
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// Desvincula o VAO (é uma boa prática desvincular qualquer buffer ou array para evitar bugs medonhos)
        glBindBuffer(GL_ARRAY_BUFFER, 0);

		// Desvincula o VAO (é uma boa prática desvincular qualquer buffer ou array para evitar bugs medonhos)
        glBindVertexArray(0);
    }
    else
    {
        cout << "Erro ao tentar ler o arquivo " << filePath << endl;
    }

    return VAO;
}

GLuint loadTexture(string filePath, int &width, int &height)
{
	GLuint texID; // id da textura a ser carregada

	// Gera o identificador da textura na memória
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);

	// Ajuste dos parâmetros de wrapping e filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Carregamento da imagem usando a função stbi_load da biblioteca stb_image
	int nrChannels;

	unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

	if (data)
	{
		if (nrChannels == 3) // jpg, bmp
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		}
		else // assume que é 4 canais png
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture " << filePath << std::endl;
	}

	stbi_image_free(data);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texID;
}

void loadConfiguration(const std::string& configPath)
{
    std::ifstream configFile(configPath);
    if (!configFile.is_open()) {
        std::cerr << "Erro ao abrir o arquivo de configuração: " << configPath << std::endl;
        return;
    }

	json config;
    configFile >> config;

	ka = config["materials"]["ka"];
    kd = config["materials"]["kd"];
    ks = config["materials"]["ks"];
	ns = config["materials"]["ns"];
	
	ambientColor = glm::vec3(
        config["materials"]["ambientColor"][0],
        config["materials"]["ambientColor"][1],
        config["materials"]["ambientColor"][2]
    );
    diffuseColor = glm::vec3(
        config["materials"]["diffuseColor"][0],
        config["materials"]["diffuseColor"][1],
        config["materials"]["diffuseColor"][2]
    );
    specularColor = glm::vec3(
        config["materials"]["specularColor"][0],
        config["materials"]["specularColor"][1],
        config["materials"]["specularColor"][2]
    );

    lightPos = glm::vec3(
        config["light"]["position"][0],
        config["light"]["position"][1],
        config["light"]["position"][2]
    );

	cameraPos = glm::vec3(
        config["camera"]["position"][0],
        config["camera"]["position"][1],
        config["camera"]["position"][2]
    );
    cameraFront = glm::vec3(
        config["camera"]["front"][0],
        config["camera"]["front"][1],
        config["camera"]["front"][2]
    );
    cameraUp = glm::vec3(
        config["camera"]["up"][0],
        config["camera"]["up"][1],
        config["camera"]["up"][2]
    );

	// lê os objetos do config file e aplica as transformações iniciais
	for (const auto& obj : config["objects"]) {
		Object newObj;
		newObj.meshes = loadSimpleOBJ(obj["file"], obj["materialFile"]);

		newObj.translacaoX = obj["transformations"]["translation"][0];
		newObj.translacaoY = obj["transformations"]["translation"][1];
		newObj.translacaoZ = obj["transformations"]["translation"][2];

		float rotation = obj["transformations"]["rotation"][0];
		newObj.fatorEscala = obj["transformations"]["scale"];
		newObj.aplicarCurva = obj["transformations"]["aplicarCurva"];

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(newObj.translacaoX, newObj.translacaoY, newObj.translacaoZ));
		model = glm::rotate(model, glm::radians(rotation), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(newObj.fatorEscala));
		for (auto& mesh : newObj.meshes) {
			mesh.model = model;
		}

		objetos.push_back(newObj);
    }
}