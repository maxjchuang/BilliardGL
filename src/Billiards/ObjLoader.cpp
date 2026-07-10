#include "ObjLoader.h"

#include "resource_path.h"

#include <GL/freeglut.h>

#include <fstream>
#include <iostream>

using namespace std;

namespace {

string directoryName(const string& path)
{
	size_t slash = path.find_last_of("/\\");
	if (slash == string::npos) {
		return "";
	}
	return path.substr(0, slash);
}

string joinPath(const string& directory, const string& fileName)
{
	if (directory.empty()) {
		return fileName;
	}
	const char last = directory[directory.size() - 1];
	if (last == '/' || last == '\\') {
		return directory + fileName;
	}
	return directory + "/" + fileName;
}

bool parseFloatValue(const string& value, float& output)
{
	try {
		output = stof(value);
		return true;
	} catch (...) {
		return false;
	}
}

bool parseIntValue(const string& value, int& output)
{
	try {
		output = stoi(value);
		return true;
	} catch (...) {
		return false;
	}
}

}  // namespace

ObjLoader::ObjLoader(string filename)
	: valid_(true), objectDirectory_(directoryName(filename))
{
	fstream f;
	f.open(filename, ios::in);
	if (!f.is_open()) {
		setError("Open obj file error: " + filename);
		cout << "Open obj file error!" << endl;
		return;
	}

	string line;
	while (getline(f, line)) {
		vector<string> parameters = splitString(line, " ");
		if (!parameters.empty() && !parseObj(parameters)) {
			break;
		}
	}
	f.close();
}

ObjLoader::~ObjLoader()
{
	for (size_t i = 0; i < materials.size(); ++i) {
		delete materials[i];
	}
}

bool ObjLoader::isValid() const
{
	return valid_;
}

const std::string& ObjLoader::error() const
{
	return error_;
}

void ObjLoader::setError(const string& message)
{
	if (valid_) {
		error_ = message;
	}
	valid_ = false;
}

vector<string> ObjLoader::splitString(std::string line, std::string delim) {
	vector<string>parameters;
	size_t pos1 = 0;
	size_t pos2 = line.find(delim);
	while (string::npos != pos2) {
		parameters.push_back(line.substr(pos1, pos2 - pos1));
		pos1 = pos2 + delim.size();
		pos2 = line.find(delim, pos1);
	}
	if (pos1 != line.length())
		parameters.push_back(line.substr(pos1));
	return parameters;
}

bool ObjLoader::parseObj(const vector<string>& parameters) {
	if (parameters.empty() || parameters[0].empty() || parameters[0][0] == '#') {
		return true;
	}

	if (parameters[0] == "v") {
		if (parameters.size() < 4) {
			setError("Invalid vertex record");
			return false;
		}
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		if (!parseFloatValue(parameters[1], x) || !parseFloatValue(parameters[2], y) || !parseFloatValue(parameters[3], z)) {
			setError("Invalid numeric value in OBJ vertex");
			return false;
		}
		positions.push_back(glm::vec3(x, y, z));
	}
	else if (parameters[0] == "vn") {
		if (parameters.size() < 4) {
			setError("Invalid normal record");
			return false;
		}
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		if (!parseFloatValue(parameters[1], x) || !parseFloatValue(parameters[2], y) || !parseFloatValue(parameters[3], z)) {
			setError("Invalid numeric value in OBJ normal");
			return false;
		}
		normals.push_back(glm::vec3(x, y, z));
	}
	else if (parameters[0] == "vt") {
		if (parameters.size() < 3) {
			setError("Invalid texture coordinate record");
			return false;
		}
		float x = 0.0f;
		float y = 0.0f;
		if (!parseFloatValue(parameters[1], x) || !parseFloatValue(parameters[2], y)) {
			setError("Invalid numeric value in OBJ texture coordinate");
			return false;
		}
		textures.push_back(glm::vec3(x, y, 0.0f));
	}
	else if (parameters[0] == "g") {
		return true;
	}
	else if (parameters[0] == "mtllib") {
		if (parameters.size() < 2) {
			setError("Missing mtllib file name");
			return false;
		}
		return parseMtl(parameters[1]);
	}
	else if (parameters[0] == "usemtl") {
		if (parameters.size() < 2) {
			setError("Missing material name");
			return false;
		}
		int index = getMtl(parameters[1]);
		if (index < 0) {
			setError("Unknown material: " + parameters[1]);
			return false;
		}
		mtlIndex.push_back(static_cast<int>(vertices.size()));
		mtlIndex.push_back(index);
	}
	else if (parameters[0] == "f") {
		if (parameters.size() != 4 && parameters.size() != 5) {
			setError("Unsupported face record");
			return false;
		}

		const int firstPassEnd = parameters.size() == 5 ? 4 : 4;
		for (int i = 1; i < firstPassEnd; i++) {
			vector<string> data = splitString(parameters[i], "/");
			if (data.size() < 3 || data[0].empty() || data[2].empty()) {
				setError("Invalid face index record");
				return false;
			}
			int positionIndex = 0;
			int textureIndex = 0;
			int normalIndex = 0;
			if (!parseIntValue(data[0], positionIndex) || !parseIntValue(data[2], normalIndex)) {
				setError("Invalid numeric value in face record");
				return false;
			}
			if (positionIndex <= 0 || positionIndex > static_cast<int>(positions.size())
				|| normalIndex <= 0 || normalIndex > static_cast<int>(normals.size())) {
				setError("Face index out of range");
				return false;
			}
			glm::vec3 texture(-1.0f, -1.0f, -1.0f);
			if (!data[1].empty()) {
				if (!parseIntValue(data[1], textureIndex)
					|| textureIndex <= 0 || textureIndex > static_cast<int>(textures.size())) {
					setError("Texture index out of range");
					return false;
				}
				texture = textures[textureIndex - 1];
			}
			vertices.push_back(Vertex(positions[positionIndex - 1], texture, normals[normalIndex - 1], parameters.size() == 5 ? 1 : 0));
		}

		if (parameters.size() == 5) {
			for (int i = 1; i < 5; i++) {
				if (i == 2) continue;
				vector<string> data = splitString(parameters[i], "/");
				if (data.size() < 3 || data[0].empty() || data[2].empty()) {
					setError("Invalid face index record");
					return false;
				}
				int positionIndex = 0;
				int textureIndex = 0;
				int normalIndex = 0;
				if (!parseIntValue(data[0], positionIndex) || !parseIntValue(data[2], normalIndex)) {
					setError("Invalid numeric value in face record");
					return false;
				}
				if (positionIndex <= 0 || positionIndex > static_cast<int>(positions.size())
					|| normalIndex <= 0 || normalIndex > static_cast<int>(normals.size())) {
					setError("Face index out of range");
					return false;
				}
				glm::vec3 texture(-1.0f, -1.0f, -1.0f);
				if (!data[1].empty()) {
					if (!parseIntValue(data[1], textureIndex)
						|| textureIndex <= 0 || textureIndex > static_cast<int>(textures.size())) {
						setError("Texture index out of range");
						return false;
					}
					texture = textures[textureIndex - 1];
				}
				vertices.push_back(Vertex(positions[positionIndex - 1], texture, normals[normalIndex - 1], 1));
			}
		}
	}
	return valid_;
}

bool ObjLoader::parseMtl(const string& filename) {
	fstream f;
	const string localPath = joinPath(objectDirectory_, filename);
	f.open(localPath, ios::in);
	if (!f.is_open()) {
		f.clear();
		f.open(billiardgl::objectPath(filename), ios::in);
	}
	if (!f.is_open()) {
		setError("Open mtl file error: " + filename);
		cout << "Open mtl file error!" << endl;
		return false;
	}

	string line;
	Material *pM = NULL;
	while (getline(f, line)) {
		if (line.size() == 0) continue;
		vector<string> parameters = splitString(line, " ");
		if (parameters.empty() || parameters[0].empty() || parameters[0][0] == '#') {
			continue;
		}
		if (parameters[0] == "newmtl") {
			if (parameters.size() < 2) {
				setError("Missing material name in MTL");
				return false;
			}
			if (pM != NULL) {
				materials.push_back(pM);
			}
			pM = new Material();
			pM->mname = parameters[1];
		}
		else if (parameters[0] == "Ka" || parameters[0] == "Kd" || parameters[0] == "Ks" || parameters[0] == "Ke") {
			if (pM == NULL) {
				setError("MTL property before newmtl: " + filename);
				return false;
			}
			if (parameters.size() < 4) {
				setError("Invalid vector material property");
				return false;
			}
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			if (!parseFloatValue(parameters[1], x) || !parseFloatValue(parameters[2], y) || !parseFloatValue(parameters[3], z)) {
				setError("Invalid numeric value in MTL");
				return false;
			}
			if (parameters[0] == "Ka") {
				pM->ambient = glm::vec3(x, y, z);
			}
			else if (parameters[0] == "Kd") {
				pM->diffuse = glm::vec3(x, y, z);
			}
			else if (parameters[0] == "Ks") {
				pM->specular = glm::vec3(x, y, z);
			}
			else {
				pM->emission = glm::vec3(x, y, z);
			}
		}
		else if (parameters[0] == "Ns") {
			if (pM == NULL) {
				setError("MTL property before newmtl: " + filename);
				return false;
			}
			if (parameters.size() < 2 || !parseIntValue(parameters[1], pM->nShininess)) {
				setError("Invalid shininess material property");
				return false;
			}
		}
		else if (parameters[0] == "map_Ka") {
			if (pM == NULL) {
				setError("MTL property before newmtl: " + filename);
				return false;
			}
			if (parameters.size() < 2) {
				setError("Invalid material texture property");
				return false;
			}
			pM->texture = parameters[1];
		}
	}
	if (pM != NULL) materials.push_back(pM);
	f.close();
	return valid_;
}

int ObjLoader::getMtl(std::string mtlName) {
	for (int i = 0; i < (int)materials.size(); i++) {
		if (mtlName == materials[i]->mname)
			return i;
	}
	return -1;
}
