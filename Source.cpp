#include <iostream>
#include <string>
#include <assert.h>

#include <vector>
#include <fstream>
#include <sstream>

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

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

int loadSimpleOBJ(string filePATH, int &nVertices);

// Dimensões da janela (pode ser alterado em tempo de execução)
const GLuint WIDTH = 1000, HEIGHT = 1000;

bool rotateX=false, rotateY=false, rotateZ=false;

//Variáveis globais da câmera
glm::vec3 cameraPos = glm::vec3(0.0f,0.0f,3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f,0.0,-1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f,1.0f,0.0f);

struct Object
{
	GLuint VAO; 
	int nVertices;
	glm::mat4 model;
	float fatorEscala = 1.0f; // valor inicial de escala
};

vector<Object> objetos;
int objSelecionado = 0; // Variável para armazenar o índice do objeto selecionado (0, 1 ou 2)

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
"    texCoord = texc;\n"
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
"uniform float ka, kd, ks, q;\n"
"\n"
"uniform vec3 lightPos, lightColor;\n"
"\n"
"uniform vec3 cameraPos;\n"
"\n"
"out vec4 color;\n"
"\n"
"void main()\n"
"{\n"
"    vec3 ambient = ka * lightColor;\n"
"\n"
"    vec3 diffuse;\n"
"    vec3 N = normalize(scaledNormal);\n"
"    vec3 L = normalize(lightPos - fragPos);\n"
"    float diff = max(dot(N,L),0.0);\n"
"    diffuse = kd * diff * lightColor;\n"
"\n"
"    vec3 specular;\n"
"    vec3 R = normalize(reflect(-L,N));\n"
"    vec3 V = normalize(cameraPos - fragPos);\n"
"    float spec = max(dot(R,V),0.0);\n"
"    spec = pow(spec,q);\n"
"    specular = ks * spec * lightColor;\n"
"\n"
"    vec3 result = (ambient + diffuse) * finalColor + specular;\n"
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

	int qntCubes = 3;
	for (int i = 0; i < qntCubes; i++) {
		Object obj;
		obj.VAO = loadSimpleOBJ("C:\\Users\\Patrick\\Desktop\\pg\\2024\\trabGA\\Visualizador3D\\cube" + to_string(i) + ".obj", obj.nVertices);
		objetos.push_back(obj);
	}

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

	//Propriedades da superfície
	shader.setFloat("ka", 0.2);
	shader.setFloat("ks", 0.5);
	shader.setFloat("kd", 0.5);
	shader.setFloat("q", 10.0);

	//Propriedades da fonte de luz
	shader.setVec3("lightPos", -2.0, 10.0, 3.0);
	shader.setVec3("lightColor", 1.0, 1.0, 1.0);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glLineWidth(10);
		glPointSize(20);

		float angle = (GLfloat)glfwGetTime();

		for (int i = 0; i < qntCubes; i++) {
			if (i == objSelecionado) {
				// Aplicar transformações apenas ao cubo selecionado
				objetos[i].model = glm::mat4(1); // Resetar a matriz

				// Aplique as rotações conforme o eixo selecionado
				if (rotateX)
				{
					objetos[i].model = glm::rotate(objetos[i].model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
				}
				else if (rotateY)
				{
					objetos[i].model = glm::rotate(objetos[i].model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
				}
				else if (rotateZ)
				{
					objetos[i].model = glm::rotate(objetos[i].model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
				}

				// Aplicar a escala
				objetos[i].model = glm::scale(objetos[i].model, glm::vec3(objetos[i].fatorEscala, objetos[i].fatorEscala, objetos[i].fatorEscala));
			}

			// Passe a matriz 'model' para o shader para cada cubo
    	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(objetos[i].model));
			
			// Atualizar a view (câmera)
			glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
			glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
			
			shader.setVec3("cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);
		
			// Renderizar todos os cubos
			glBindVertexArray(objetos[i].VAO);
			glDrawArrays(GL_TRIANGLES, 0, objetos[i].nVertices);
		}

		glfwSwapBuffers(window);
	}

	// glDeleteVertexArrays(1, &objSelecionado.VAO);
	for (int i = 0; i < qntCubes; i++) {
		glDeleteVertexArrays(1, &objetos[i].VAO);
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
	}
}

int loadSimpleOBJ(string filePath, int &nVertices)
{
	GLuint VBO, VAO;
	vector <glm::vec3> vertices;
	vector <glm::vec2> texCoords;
	vector <glm::vec3> normals;

	vector <GLfloat> vBuffer;
	glm::vec3 color = glm::vec3(1.0, 0.0, 0.0);

	ifstream arqEntrada;

	arqEntrada.open(filePath.c_str());
	if (arqEntrada.is_open())
	{
		string line;
		while (getline(arqEntrada, line))
		{
			istringstream ssline(line);
			string word;
			ssline >> word;
			if (word == "v")
			{
				glm::vec3 vertice;
				ssline >> vertice.x >> vertice.y >> vertice.z;
				vertices.push_back(vertice);
			}
			if (word == "vt")
			{
				glm::vec2 vt;
				ssline >> vt.s >> vt.t;
				texCoords.push_back(vt);
			}
			if (word == "vn")
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
	else
	{
		cout << "Erro ao tentar ler o arquivo " << filePath << endl;
		return -1;
	}
}
