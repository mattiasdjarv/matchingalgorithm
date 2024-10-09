#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <Python.h>
#define datasetSize 2221
#define pvValue 0
#define batteryValue 1
#define consValue 2
#define weightValue 3
#define Range 40000


//g++ throws a bunch of warnings over these 
//but they don't cause any issues as far as I can tell
#define runopt "runopt"
#define runOptimize "runOptimize"
#define neighborHood "neighborhood"
#define findNeighborhood "findNeighborhood"


using namespace std;


PyObject *makelist(int array[], size_t size);
int readFile(string path, int type);
int preprocess(bool printValues);
int greedyMatching();
int pythonOptimizer(char *name, char *function, int indexCount, int indexes[]);
int pythonNeighborhood(char *name, char *function, int index, int range);


typedef struct
{
	float	pv;
	float	battery;
	float	cons;
	int		matchedToIndex;
}*Prosumer;

struct 
{
	float	pv[datasetSize];
	float	battery[datasetSize];
	float	cons[datasetSize];
	Prosumer prosumers[datasetSize];
	bool  	availableConsumers[datasetSize];
	float 	currentWeight;
	int		neighbors[datasetSize];
	int		neighborCount;
} myData;

PyObject *pName, *pModule, *pFunc;
PyObject *pArgs, *pValue;

int main(int argc, char *argv[]) {
    Py_Initialize();
	
	//These 2 lines just allow the interpreter to access python files in the current directory
	//this means that the shell running the process has to be in python/ even if the executable is in another e.g. python/build/
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append(\".\")");
	
	myData.neighborCount = 0; //simple init value to avoid undefined behavior
	pythonNeighborhood(neighborHood, findNeighborhood, 0, 40000);
	
	preprocess(false);
	greedyMatching();
	
	for(int i=0; i<datasetSize; i++){
		if(myData.prosumers[i] == NULL){
			continue;
		}
		else{
			//no point displaying prosumers who didn't match for now
			if(myData.prosumers[i]->matchedToIndex == -1){
				break;
			}
		}
		cout << "Prosumer " << i << "is matched to consumer " << myData.prosumers[i]->matchedToIndex << endl;
	}
	
	if (Py_FinalizeEx() < 0) {
        return 120;
    }
	
    return EXIT_SUCCESS;
}

int greedyMatching(){
	
	// change the i<value to make it run faster when debugging
	for(int i=0; i<10; i++){
		if(myData.prosumers[i] == NULL){
			continue;
		}
		
		
		//finds the neighborhood via a python function see below for indepth
		pythonNeighborhood(neighborHood, findNeighborhood, i, Range);
		
		
		float	currentBestWeight	= -1;
		int 	currentBestIndex	= -1;
		
		/*
		debug code to check the neighborhoods of the prosumers
		cout << "Prosumer " << i << " has the following neighbors: " << endl;
		for(int j=0; j<myData.neighborCount; j++){
			cout << myData.neighbors[j] << endl;
		}
		cout << "\n" << endl;
		cout << "\n" << endl;
		continue;
		*/
		
		//when empty it simply makes no match which is equivalent to having no neighbors
		for(int j=0; j<myData.neighborCount; j++){
			//checking if edge j in the neighborhood is available
			
			/*
			something segfaults without either of prints surronding the following if statement
			changing to
			cout << "Current weight is: "<< myData.currentWeight << endl;
			from
			cout << "Current weight is: "<< myData.currentWeight << "\n";
			seems to reslove the issue which is weird because it wasn't there before
			*/
			//cout << "before" << endl;
			if(!myData.availableConsumers[myData.neighbors[j]]){
				continue;
			}
			//cout << "after" << endl;
			
			int list[2] = {i,j};
			pythonOptimizer(runopt, runOptimize, 2, list);
			cout << "Current weight is: "<< myData.currentWeight << endl;
			if(currentBestWeight<myData.currentWeight){
				currentBestWeight = myData.currentWeight;
				currentBestIndex  = j;
			}
		}
		cout << "best index: " << currentBestIndex << "\nbest weight: " << currentBestWeight << endl;
		
		//matching the prosumer to its consumer and marking the consumer as unavailable
		myData.prosumers[i]->matchedToIndex = currentBestIndex;
		myData.availableConsumers[currentBestIndex] = false;	
	}
	return EXIT_SUCCESS;
}


//prepares the data for the matching
//calling it with true will print all values used for the matching
int preprocess(bool printValues){
	//this extracts the pv, battery and cons values to the files
	if (system("python3 preprocess.py"))
	{
		cout << "Failed to run extract" << endl;
		return EXIT_FAILURE;
	}
	
	//retrieves the pv and battery values from the files and optionally prints them all out
	//could probably generalize the following more but not sure how much of this stays
	if(readFile("pvdata.txt",pvValue)){
		cout << "error couldn't read pvdata.txt\n";
	}
	else{
		if(printValues){
			cout << "pv values: \n";
			for(int i = 0; i<datasetSize; i++){
				cout << myData.pv[i] << "\t";
			}
			cout << "\n\n";
		}
	}
	if(readFile("batterydata.txt",batteryValue)){
		cout << "error couldn't read batterydata.txt\n";
	}
	else{
		if(printValues){
			cout << "battery values: \n";
			for(int i = 0; i<datasetSize; i++){
				cout << myData.battery[i] << "\t";
			}
			cout << "\n\n";
		}
	}
	
	// The file this reads is huge so ignored for now
	/*
	if(readFile("consdata.txt",consValue)){
		cout << "error couldn't read consdata.txt\n";
	}
	else{
		if(printValues){
			cout << "consumption values: \n";
			for(int i = 0; i<100; i++){
				cout << myData.cons[i] << "\t";
			}
			cout << "\n\n";
		}
	}
	*/
	
	//initalizes the prosumer and consumer sets
	for(int i=0; i<datasetSize; i++){
		myData.prosumers[i] = NULL;
		myData.availableConsumers[i] = false;
	}
	
	//int counter = 0;
	for(int i=0; i<datasetSize; i++){
		if(myData.pv[i]!=0 || myData.battery[i]!=0){
			//initalizes a new pointer for every prosumer tuple
			Prosumer prosumer = (Prosumer)malloc(sizeof(Prosumer));
			prosumer->pv = myData.pv[i];
			prosumer->battery = myData.battery[i];
			prosumer->matchedToIndex = -1;
			myData.prosumers[i] = prosumer;
			
			/* 	
				The following makes a condensed array instead of a sparse array
				A dense array means less values to check when running the alg
				However NULL checking should be fast so they are probably about equal
				
				*** Make sure to uncomment the //int counter = 0; above and change the NULL checks to break instead of continue in that case ***
			*/
			//myData.prosumers[counter] = prosumer;
			//counter++;
		}
		else{
			myData.availableConsumers[i] = true;
		}
	}
	//prints the prosumer and consumer set
	if(printValues){
		cout << "Index values of prosumers: \n";
			for(int i = 0; i<datasetSize; i++){
				if(myData.prosumers[i] == NULL){
					//break;
					continue;
				}
				cout << i << "\t";
			}
		cout << "\n\n";
		
		cout << "Index values of available consumers: \n";
			for(int i = 0; i<datasetSize; i++){
				if(!myData.availableConsumers[i]){
					//break;
					continue;
				}
				cout << i << "\t";
			}
		cout << "\n\n";
	}
	return EXIT_SUCCESS;
}

//reads in the data from a a file
int readFile(string path, int type){
	ifstream file;
	
	if(!filesystem::exists(path)){
		return EXIT_FAILURE;
	}
	
	file.open(path);
	if(!file.is_open()){
		cout << "error\n";
		return EXIT_FAILURE;
	}
	int i = 0;
	string tmp;
	
	while (file >> tmp){
		if(type == pvValue){
			myData.pv[i] = stof(tmp);
		}
		else if(type == batteryValue){
			myData.battery[i] = stof(tmp);
		}
		else if(type == consValue){
			myData.cons[i] = stof(tmp);
		}
		else if(type == weightValue){
			myData.currentWeight = stof(tmp);
		}
		i++;
	}
	file.close();
	return EXIT_SUCCESS;
}

PyObject *makelist(int array[], int size) {
    PyObject *l = PyList_New(size);
    for (int i = 0; i < size; ++i) {
        PyList_SET_ITEM(l, i, PyLong_FromLong(array[i]));
    }
    return l;
}

int *makearr(PyObject *list, int* arr, int size) {
    for (int i = 0; i < size; ++i) {
        arr[i] = PyLong_AsLong(PyList_GetItem(list, i));
    }
    return EXIT_SUCCESS;
}

//calls the optimizer python function and returns the result
int pythonOptimizer(char *name, char *function, int indexCount, int indexes[]){
	
	pName = PyUnicode_DecodeFSDefault(name);
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, function);
        /* pFunc is a new reference */

        if (pFunc && PyCallable_Check(pFunc)) {
			pArgs = PyTuple_New(2);
			PyTuple_SetItem(pArgs, 0, PyLong_FromLong(indexCount));
			PyObject *pList;
			pList = makelist(indexes,indexCount);
			PyTuple_SetItem(pArgs, 1, pList);
			
            pValue = PyObject_CallObject(pFunc, pArgs);
			Py_DECREF(pList);
            Py_DECREF(pArgs);
			
            if (pValue != NULL) {
				//I think PyFloat_AsDouble reduces us to a double from a long double so we are losing accuracy here but it shouldn't really matter at all
                //printf("Result of call: %f\n", PyFloat_AsDouble(pValue));
				myData.currentWeight = PyFloat_AsDouble(pValue);
                Py_DECREF(pValue);
				
            }
            else {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                fprintf(stderr,"Call failed\n");
                return EXIT_FAILURE;
            }
        }
        else {
            if (PyErr_Occurred())
                PyErr_Print();
            fprintf(stderr, "Cannot find function \"%s\"\n", function);
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
		return EXIT_SUCCESS;
    }
    else {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", name);
        return EXIT_FAILURE;
    }
}

//calls a python function that returns the neighborhood for a consumer/prosumer
//the neighborhood is an unsorted array of indexes which is stored inside myData.neighbors
//myData.neighbors only lazy deletes so only the first myData.neighborCount elements are actually valid at any time
int pythonNeighborhood(char *name, char *function, int index, int range){
	
	pName = PyUnicode_DecodeFSDefault(name);
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, function);
        /* pFunc is a new reference */

        if (pFunc && PyCallable_Check(pFunc)) {
			pArgs = PyTuple_New(2);
			PyTuple_SetItem(pArgs, 0, PyLong_FromLong(range));
			PyTuple_SetItem(pArgs, 1, PyLong_FromLong(index));
			
            pValue = PyObject_CallObject(pFunc, pArgs);
            Py_DECREF(pArgs);
			
            if (pValue != NULL) {
				int size = PyList_Size(pValue);
				int array[size];
				makearr(pValue, array, size);
				
				//saving the data to permanent variables
				myData.neighborCount = size;
				for (int i = 0; i < size; ++i) {
					myData.neighbors[i] = array[i];
				}
				
				//GC
                Py_DECREF(pValue);
				
            }
            else {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                fprintf(stderr,"Call failed\n");
                return EXIT_FAILURE;
            }
        }
        else {
            if (PyErr_Occurred())
                PyErr_Print();
            fprintf(stderr, "Cannot find function \"%s\"\n", function);
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
		return EXIT_SUCCESS;
    }
    else {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", name);
        return EXIT_FAILURE;
    }
}