

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <symtab.h>
#include "semant.h"
#include "utilities.h"

#include <vector>
#include <map>
#include <string>
#include <algorithm>


extern int semant_debug;
extern char *curr_filename;

//////////////////////////////////////////////////////////////////////
//
// Symbols
//
// For convenience, a large number of symbols are predefined here.
// These symbols include the primitive type and method names, as well
// as fixed names used by the runtime system.
//
//////////////////////////////////////////////////////////////////////
static Symbol 
    arg,
    arg2,
    Bool,
    concat,
    cool_abort,
    copy,
    Int,
    in_int,
    in_string,
    IO,
    length,
    Main,
    main_meth,
    No_class,
    No_type,
    Object,
    out_int,
    out_string,
    prim_slot,
    self,
    SELF_TYPE,
    Str,
    str_field,
    substr,
    type_name,
    val;
//
// Initializing the predefined symbols.
//
static void initialize_constants(void)
{
    arg         = idtable.add_string("arg");
    arg2        = idtable.add_string("arg2");
    Bool        = idtable.add_string("Bool");
    concat      = idtable.add_string("concat");
    cool_abort  = idtable.add_string("abort");
    copy        = idtable.add_string("copy");
    Int         = idtable.add_string("Int");
    in_int      = idtable.add_string("in_int");
    in_string   = idtable.add_string("in_string");
    IO          = idtable.add_string("IO");
    length      = idtable.add_string("length");
    Main        = idtable.add_string("Main");
    main_meth   = idtable.add_string("main");
    //   _no_class is a symbol that can't be the name of any 
    //   user-defined class.
    No_class    = idtable.add_string("_no_class");
    No_type     = idtable.add_string("_no_type");
    Object      = idtable.add_string("Object");
    out_int     = idtable.add_string("out_int");
    out_string  = idtable.add_string("out_string");
    prim_slot   = idtable.add_string("_prim_slot");
    self        = idtable.add_string("self");
    SELF_TYPE   = idtable.add_string("SELF_TYPE");
    Str         = idtable.add_string("String");
    str_field   = idtable.add_string("_str_field");
    substr      = idtable.add_string("substr");
    type_name   = idtable.add_string("type_name");
    val         = idtable.add_string("_val");

}

int findCycle(std::vector< std::vector<int> > graph, std::vector<int>& inCycle) {
    // Since multiple inheritance is not allowed, we can just check if
    // the whole inheritance graph is connected. If it is, it is valid.
    // If it's not connected, it isn't valid.
    std::vector<int> visited = std::vector<int>(graph.size(), 0);
    std::vector<int> toVisit;
    inCycle.clear();

    toVisit.push_back(0);

    while(!toVisit.empty()) {
        int current = toVisit.back();
        toVisit.pop_back();

        if(visited[current]) continue;

        visited[current] = 1;

        for(unsigned int i = 0; i < graph.size(); i++) {
            if(graph[i][current] && !visited[i]) toVisit.push_back(i);
        }
    }

    for(unsigned int i = 0; i < visited.size(); i++) {
        if(!visited[i]) inCycle.push_back(i);
    }

    return inCycle.size();
}

// This creates the empty class list and checks the inheritance graph for errors
ClassTable::ClassTable(Classes classes) : semant_errors(0) , error_stream(cerr) {

    classList = nil_Classes();

    // First we install the basic classes
    install_basic_classes();

    // Now we add the user-defined classes
    classList = Classes_class::append(classList, classes);

    // Map the classes
    std::map<std::string, int> indices;
    for(int i = classList->first(); classList->more(i); i = classList->next(i)) {
        std::string thisName = classList->nth(i)->get_name()->get_string();
        idtable.add_string(classList->nth(i)->get_name()->get_string());
        if(indices.count(thisName) > 0) {
            std::string fileName = std::string(classList->nth(i)->get_filename()->get_string());
            int linenumber = classList->nth(i)->get_line_number();
            semant_error() << fileName << ":" << linenumber << ": Class "
                           << thisName << " was previously defined.\n";
        } else {
            indices[thisName] = i;
        }
    }

    // Create the inheritance graph
    std::vector< std::vector<int> > G;
    for(int i = classList->first(); classList->more(i); i = classList->next(i)) {
        G.push_back(std::vector<int>(classList->len(), 0));
        std::string thisName = std::string(classList->nth(i)->get_name()->get_string());
        std::string parentName = std::string(classList->nth(i)->get_parent()->get_string());
        std::string fileName = std::string(classList->nth(i)->get_filename()->get_string());
        int linenumber = classList->nth(i)->get_line_number();
        if(parentName != "_no_class") {
            // Inheritance from invalid class
            if(!indices.count(parentName)) {
                semant_error() << fileName << ":" << linenumber << ": Class "
                               << thisName << " inherits from an undefined class "
                               << parentName << ".\n";
            } else if(parentName == "Int" || parentName == "String" || parentName == "Bool" ) {
                semant_error() << fileName << ":" << linenumber << ": Class "
                               << thisName << " cannot inherit class "
                               << parentName << ".\n";
            } else {
                G[indices[thisName]][indices[parentName]] = 1;
            }
        }
    }

    std::vector<int> cycleClasses;
    if(!errors() && findCycle(G, cycleClasses)) {
        for(int i = cycleClasses.size() - 1; i >= 0; i--) {
            int index = cycleClasses[i];
            std::string thisName = classList->nth(index)->get_name()->get_string();
            std::string fileName = classList->nth(index)->get_filename()->get_string();
            int linenumber = classList->nth(index)->get_line_number();
            semant_error() << fileName << ":" << linenumber << ": Class "
                           << thisName << ", or an ancestor of " << thisName
                           << ", is involved in an inheritance cycle.\n";
        }
    }

}

void ClassTable::install_basic_classes() {

    // The tree package uses these globals to annotate the classes built below.
    curr_lineno  = 0;
    Symbol filename = stringtable.add_string("<basic class>");
    
    // The following demonstrates how to create dummy parse trees to
    // refer to basic Cool classes.  There's no need for method
    // bodies -- these are already built into the runtime system.
    
    // IMPORTANT: The results of the following expressions are
    // stored in local variables.  You will want to do something
    // with those variables at the end of this method to make this
    // code meaningful.

    // 
    // The Object class has no parent class. Its methods are
    //        abort() : Object    aborts the program
    //        type_name() : Str   returns a string representation of class name
    //        copy() : SELF_TYPE  returns a copy of the object
    //
    // There is no need for method bodies in the basic classes---these
    // are already built in to the runtime system.

    Class_ Object_class =
	class_(Object, 
	       No_class,
	       append_Features(
			       append_Features(
					       single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
					       single_Features(method(type_name, nil_Formals(), Str, no_expr()))),
			       single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))),
	       filename);

    // 
    // The IO class inherits from Object. Its methods are
    //        out_string(Str) : SELF_TYPE       writes a string to the output
    //        out_int(Int) : SELF_TYPE            "    an int    "  "     "
    //        in_string() : Str                 reads a string from the input
    //        in_int() : Int                      "   an int     "  "     "
    //
    Class_ IO_class = 
	class_(IO, 
	       Object,
	       append_Features(
			       append_Features(
					       append_Features(
							       single_Features(method(out_string, single_Formals(formal(arg, Str)),
										      SELF_TYPE, no_expr())),
							       single_Features(method(out_int, single_Formals(formal(arg, Int)),
										      SELF_TYPE, no_expr()))),
					       single_Features(method(in_string, nil_Formals(), Str, no_expr()))),
			       single_Features(method(in_int, nil_Formals(), Int, no_expr()))),
	       filename);  

    //
    // The Int class has no methods and only a single attribute, the
    // "val" for the integer. 
    //
    Class_ Int_class =
	class_(Int, 
	       Object,
	       single_Features(attr(val, prim_slot, no_expr())),
	       filename);

    //
    // Bool also has only the "val" slot.
    //
    Class_ Bool_class =
	class_(Bool, Object, single_Features(attr(val, prim_slot, no_expr())),filename);

    //
    // The class Str has a number of slots and operations:
    //       val                                  the length of the string
    //       str_field                            the string itself
    //       length() : Int                       returns length of the string
    //       concat(arg: Str) : Str               performs string concatenation
    //       substr(arg: Int, arg2: Int): Str     substring selection
    //       
    Class_ Str_class =
	class_(Str, 
	       Object,
	       append_Features(
			       append_Features(
					       append_Features(
							       append_Features(
									       single_Features(attr(val, Int, no_expr())),
									       single_Features(attr(str_field, prim_slot, no_expr()))),
							       single_Features(method(length, nil_Formals(), Int, no_expr()))),
					       single_Features(method(concat, 
								      single_Formals(formal(arg, Str)),
								      Str, 
								      no_expr()))),
                               single_Features(method(substr,
                                                      append_Formals(single_Formals(formal(arg, Int)),
                                                                     single_Formals(formal(arg2, Int))),
                                                      Str,
                                                      no_expr()))),
	       filename);


    // In the end we add the basic classes to the class list
    classList = Classes_class::append(classList, single_Classes(Object_class));
    classList = Classes_class::append(classList, single_Classes(IO_class));
    classList = Classes_class::append(classList, single_Classes(Int_class));
    classList = Classes_class::append(classList, single_Classes(Bool_class));
    classList = Classes_class::append(classList, single_Classes(Str_class));
}

////////////////////////////////////////////////////////////////////
//
// semant_error is an overloaded function for reporting errors
// during semantic analysis.  There are three versions:
//
//    ostream& ClassTable::semant_error()                
//
//    ostream& ClassTable::semant_error(Class_ c)
//       print line number and filename for `c'
//
//    ostream& ClassTable::semant_error(Symbol filename, tree_node *t)  
//       print a line number and filename
//
///////////////////////////////////////////////////////////////////

ostream& ClassTable::semant_error(Class_ c)
{                                                             
    return semant_error(c->get_filename(),c);
}    

ostream& ClassTable::semant_error(Symbol filename, tree_node *t)
{
    error_stream << filename << ":" << t->get_line_number() << ": ";
    return semant_error();
}

ostream& ClassTable::semant_error()                  
{                                                 
    semant_errors++;                            
    return error_stream;
}

// Checks if the given class exists and returns it
Class__class* ClassTable::lookup(std::string className) {
    for(int i = classList->first(); classList->more(i); i = classList->next(i)) {
        std::string thisName = std::string(classList->nth(i)->get_name()->get_string());

        if(thisName == className) return classList->nth(i);
    }

    return NULL;
}

// Checks if a given class inherits from another (directly or indirectly)
int ClassTable::inheritsFrom(std::string child, std::string parent) {
    if(child == parent) return 1;
    if(child == "Object") return 0;

    for(int i = classList->first(); classList->more(i); i = classList->next(i)) {
        std::string thisName = std::string(classList->nth(i)->get_name()->get_string());
        std::string parentName = std::string(classList->nth(i)->get_parent()->get_string());

        if(thisName == child) {
            if(parentName == parent) {
                return 1;
            } else {
                return inheritsFrom(parentName, parent);
            }
        }
    }

    return 0;
}

// Finds the most specific class which is a parent to both class c1 and c2
Class__class* ClassTable::nearestCommonParent(std::string c1, std::string c2) {
    std::vector<char*> visited = std::vector<char*>();
    Class__class* currentClass = lookup(c1);

    while(std::string(currentClass->get_parent()->get_string()) != "_no_class") {
        char* currentName = currentClass->get_name()->get_string();
        char* parentName = currentClass->get_parent()->get_string();
        visited.push_back(currentName);

        currentClass = lookup(parentName);
    }
    visited.push_back("Object");

    currentClass = lookup(c2);
    while(std::string(currentClass->get_parent()->get_string()) != "_no_class") {
        char* currentName = currentClass->get_name()->get_string();
        char* parentName = currentClass->get_parent()->get_string();

        if(std::find(visited.begin(), visited.end(), currentName) != visited.end()) {
            return currentClass;
        }

        currentClass = lookup(parentName);
    }

    return lookup("Object");
}

// Finds a method belonging to c or one of it's ancestors
Feature_class* ClassTable::findMethod(std::string c, std::string m) {
    Feature_class* found = NULL;
    Class__class* currentClass;
    std::string parentClass = c;

    do {
        currentClass = lookup(parentClass);

        Features f = currentClass->get_features();
        for(int i = f->first(); f->more(i); i = f->next(i)) {
            if(f->nth(i)->isMethod() && std::string(f->nth(i)->get_name()->get_string()) == m) {
                found = f->nth(i);
            }
        }

        parentClass = currentClass->get_parent()->get_string();
    } while(parentClass != "_no_class");

    return found;
}


void ClassTable::semanticAnalysis() {
    for(int i = classList->first(); classList->more(i); i = classList->next(i)) {
        classList->nth(i)->semant(*this);
    }
}


// Semantic analysis for a single class
void class__class::semant(ClassTable& classes) {
    SymbolTable<char*, Feature_class> *methods = new SymbolTable<char*, Feature_class>();
    SymbolTable<std::string, char*> *variables = new SymbolTable<std::string, char*>();

    variables->enterscope();
    methods->enterscope();

    char* currentClassName = this->get_name()->get_string();
    variables->addid(self->get_string(), &currentClassName);
    // First we store the available methods and attributes
    for(int i = features->first(); features->more(i); i = features->next(i)) {
        char* featName = features->nth(i)->get_name()->get_string();

        if(features->nth(i)->isMethod()) {
            if(methods->probe(featName) == NULL) {
                methods->addid(idtable.lookup_string(featName)->get_string(), features->nth(i));
                idtable.add_string(featName);
            } else {
                classes.semant_error() << get_filename() << ":" << features->nth(i)->get_line_number()
                     << ": Method " << featName << " is multiply defined.\n";
            }
        } else {
            if(variables->probe(featName) == NULL) {
                char** ftype = new char*;
                *ftype = features->nth(i)->get_ftype()->get_string();
                variables->addid(idtable.lookup_string(featName)->get_string(),
                                 ftype);
                idtable.add_string(featName);
            } else {
                classes.semant_error() << get_filename() << ":" << features->nth(i)->get_line_number()
                     << ": Attribute " << featName << " is multiply defined.\n";
            }
        }
    }

    // Finally we check to see if any methods were incorrectly overwritten
    for(int i = features->first(); features->more(i); i = features->next(i)) {
        Class__class* current = this;
        Feature_class* overwritten = NULL;

        do {
            if(std::string(current->get_parent()->get_string()) == "_no_class") break;
            current = classes.lookup(current->get_parent()->get_string());
            Features currentFeats = current->get_features();

            for(int j = currentFeats->first(); currentFeats->more(j); j = currentFeats->next(j)) {
                if(currentFeats->nth(j)->isMethod() &&
                   std::string(currentFeats->nth(j)->get_name()->get_string()) ==
                   std::string(features->nth(i)->get_name()->get_string())
                   ) {
                    overwritten = currentFeats->nth(j);
                    break;
                }
            }

        } while(std::string(current->get_parent()->get_string()) != "_no_class");

        if(overwritten != NULL) {
            Formals currentFormals = features->nth(i)->get_formals();
            Formals overFormals = overwritten->get_formals();

            if(std::string(features->nth(i)->get_ftype()->get_string()) !=
               std::string(overwritten->get_ftype()->get_string())) {
                classes.semant_error() << get_filename() << ":" << get_line_number()
                     << ": In redefined method " << features->nth(i)->get_name()->get_string()
                     << ", return type " << features->nth(i)->get_ftype()->get_string()
                     << " is different from original return type "
                     << overwritten->get_ftype()->get_string() << ".\n";
            } else if(overFormals->len() != currentFormals->len()) {
                classes.semant_error() << get_filename() << ":" << get_line_number()
                     << ": Incompatible number of formal parameters in redefined method "
                     << features->nth(i)->get_name()->get_string() << ".\n";

            } else {
                for(int j = currentFormals->first(), k = overFormals->first();
                    currentFormals->more(j);
                    j = currentFormals->next(j), k = overFormals->next(k)) {

                    if(std::string(overFormals->nth(k)->get_type_decl()->get_string()) !=
                        std::string(currentFormals->nth(j)->get_type_decl()->get_string())
                        ) {
                        // Formals don't match
                        classes.semant_error() << get_filename() << ":" << get_line_number()
                             << ": In redefined method " << features->nth(i)->get_name()->get_string()
                             << ", parameter type " << currentFormals->nth(j)->get_type_decl()->get_string()
                             << " is different from original type "
                             << overFormals->nth(k)->get_type_decl()->get_string() << "\n";
                        break;

                    }
                }
            }

        }
    }

    // Now we do the semantic analysis for each feature
    for(int i = features->first(); features->more(i); i = features->next(i)) {
        features->nth(i)->semant(classes, *variables, this);
    }

    variables->exitscope();
    methods->exitscope();

    delete variables;
    delete methods;
}

// Semantic analysis for an attribute
void attr_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {
    Expression init = get_expr();
    int success = init->semant(classes, variables, currentClass);

    if(init->get_type() != NULL) {
        // We have an initialization
        std::string declared_type = std::string(type_decl->get_string());
        std::string actual_type = std::string(init->get_type()->get_string());

        if(declared_type == "SELF_TYPE") declared_type =  currentClass->get_name()->get_string();

        if(classes.lookup(declared_type) == NULL) {
            classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
                 << ": Class " << declared_type << " of attribute "
                 << get_name() << " is undefined.\n";
        }

        if(success && actual_type != "_no_type"){

            if(classes.lookup(declared_type) != NULL) {
                if(!classes.inheritsFrom(actual_type, declared_type)) {
                    classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
                         << ": Inferred type " << actual_type << " of initialization of"
                         << "attribute " << get_name() << " does not conform to declared type "
                         << type_decl->get_string() << ".\n";
                }
            }

        }
    }
}

// Semantic analysis for a method
void method_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    variables.enterscope();

    // Add the function formal parameters to the symbol table
    Formals f = get_formals();
    for(int i = formals->first(); formals->more(i); i = formals->next(i)) {

        std::string formalName = formals->nth(i)->get_name()->get_string();
        char** formalType = new char*;
        *formalType = formals->nth(i)->get_type_decl()->get_string();

        if(classes.lookup(*formalType) == NULL) {
            classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
                 << ": Class " << *formalType << " of formal parameter "
                 << formalName << " is undefined.\n";
        }

        if(variables.probe(formalName) == NULL) {
            variables.addid(formalName, formalType);
            idtable.add_string(formals->nth(i)->get_name()->get_string());
        } else {
            classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
                 << ": Formal parameter " << formalName << " is multiply defined.\n";
        }
    }


    // Evaluate method body and check if its type corresponds to the method type
    get_expr()->semant(classes, variables, currentClass);

    // We must not check basic classes' methods
    if(get_expr()->get_type() != NULL) {

        char* expressionType = get_expr()->get_type()->get_string();
        char* methodType = get_ftype()->get_string();

        if(std::string(methodType) == "SELF_TYPE") methodType = currentClass->get_name()->get_string();

        if(classes.lookup(methodType) != NULL) {
            if(!classes.inheritsFrom(expressionType, methodType)) {
                classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
                     << ": Inferred return type " << expressionType
                     << " of method " << get_name()->get_string()
                     << " does not conform to declared return type "
                     << get_ftype()->get_string() << ".\n";
            }
        } else {
            classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
                 << ": Undefined return type " << get_ftype()->get_string()
                 << " in method " << get_name()->get_string() << ".\n";
        }

    }

    variables.exitscope();
}

// Semantic analysis for a case branch
void branch_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {
    variables.enterscope();

    std::string varName = get_name()->get_string();
    std::string varType = get_type_decl()->get_string();
    if(varType == "SELF_TYPE") varType = currentClass->get_name()->get_string();

    if(classes.lookup(varType) == NULL) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Class " << varType << " of case branch "
             << varName << " is undefined.\n";
    }

    char** vType = new char*;
    *vType = get_type_decl()->get_string();
    if(std::string(*vType) == "SELF_TYPE") *vType = currentClass->get_name()->get_string();

    variables.addid(get_name()->get_string(), vType);
    idtable.add_string(get_name()->get_string());

    get_expr()->semant(classes, variables, currentClass);

    variables.exitscope();
}

// Semantic analysis for an assignment
int assign_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {
    // We evaluate the expression on the right hand side
    int subResult = get_expr()->semant(classes, variables, currentClass);

    char* leftType = *variables.lookup(get_name()->get_string());
    char* rightType = get_expr()->get_type()->get_string();

    if(leftType == NULL) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Assignment to undeclared variable "
             << get_name()->get_string() << ".\n";
    }

    if(subResult && leftType != NULL && classes.lookup(leftType) != NULL &&
        !classes.inheritsFrom(rightType, leftType)) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Type " << rightType << " of assigned expression does not "
             << "conform to declared type " << leftType << " of identifier "
             << get_name()->get_string() << ".\n";

        set_type(idtable.lookup_string("Object"));
        return 0;
    }

    set_type(idtable.lookup_string(leftType));

    return subResult;
}

// Semantic analysis for static method dispatch
int static_dispatch_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {
    // We evaluate the expression on the left hand side
    int success = expr->semant(classes, variables, currentClass);

    for(int i = actual->first(); actual->more(i); i = actual->next(i)) {
        success = actual->nth(i)->semant(classes, variables, currentClass) && success;
    }

    std::string leftType = expr->get_type()->get_string();

    if(leftType == "SELF_TYPE") leftType = currentClass->get_name()->get_string();

    std::string staticType = type_name->get_string();

    // Does not allow method call to static type "SELF_TYPE"
    if(staticType == "SELF_TYPE") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Static dispatch to SELF_TYPE.\n";

        set_type(idtable.lookup_string("Object"));

        return 0;
    }

    // Check if left hand side type conforms to specified static type
    if(!classes.inheritsFrom(leftType, staticType)) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Expression type " << leftType
             << " does not conform to declared static dispatch type "
             << staticType << ".\n";

        set_type(idtable.lookup_string("Object"));

        return 0;
    }

    Feature_class* m = classes.findMethod(staticType, name->get_string());

    if(m == NULL) {
        // No matching method found
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Static dispatch to undefined method " << name->get_string() << ".\n";

        set_type(idtable.lookup_string("Object"));

        return 0;
    } 

    std::string returnType = m->get_ftype()->get_string();
    set_type(idtable.lookup_string(m->get_ftype()->get_string()));
    if(returnType == "SELF_TYPE") {
        returnType = staticType;
        set_type(idtable.lookup_string(type_name->get_string()));
    }

    Formals form = m->get_formals();

    // Check if the number of parameters is the same
    if(form->len() != actual->len()) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Method " << name->get_string() << " called with wrong number of arguments.\n";

        return 0;
    }    

    // Check if parameters match
    int match = 1;
    for(int i = form->first(), j = actual->first();
        form->more(i);
        i = form->next(i), j = actual->next(j)) {

        if(!classes.inheritsFrom(actual->nth(j)->get_type()->get_string(),
            form->nth(i)->get_type_decl()->get_string())
            ) {
            // Parameters don't match
            classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
                 << ": In call of method " << name->get_string()
                 << ", type " << actual->nth(j)->get_type()->get_string()
                 << " of parameter " << form->nth(i)->get_name()->get_string()
                 << " does not conform to declared type " << form->nth(i)->get_type_decl()->get_string()
                 << ".\n";

            match = 0;
        }
    }

    return match;

}

// Semantic analysis for method dispatch
int dispatch_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {
    // We evaluate the expression on the left hand side
    int success = expr->semant(classes, variables, currentClass);

    for(int i = actual->first(); actual->more(i); i = actual->next(i)) {
        success = actual->nth(i)->semant(classes, variables, currentClass) && success;
    }

    std::string leftType = expr->get_type()->get_string();

    if(leftType == "SELF_TYPE") leftType = currentClass->get_name()->get_string();

    Feature_class* m = classes.findMethod(leftType, name->get_string());

    if(m == NULL) {
        // No matching method found
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Dispatch to undefined method " << name->get_string() << ".\n";

        set_type(idtable.lookup_string("Object"));

        return 0;
    } 

    std::string returnType = m->get_ftype()->get_string();
    set_type(idtable.lookup_string(m->get_ftype()->get_string()));
    if(returnType == "SELF_TYPE") {
        returnType = leftType;
        set_type(idtable.lookup_string(expr->get_type()->get_string()));
        if(leftType == "SELF_TYPE") {
            set_type(idtable.lookup_string(currentClass->get_name()->get_string()));
        }
    }

    Formals form = m->get_formals();

    // Check if the number of parameters is the same
    if(form->len() != actual->len()) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Method " << name->get_string() << " called with wrong number of arguments.\n";

        return 0;
    }    

    // Check if parameters match
    int match = 1;
    for(int i = form->first(), j = actual->first();
        form->more(i);
        i = form->next(i), j = actual->next(j)) {

        if(!classes.inheritsFrom(actual->nth(j)->get_type()->get_string(),
            form->nth(i)->get_type_decl()->get_string())
            ) {
            // Parameters don't match
            classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
                 << ": In call of method " << name->get_string()
                 << ", type " << actual->nth(j)->get_type()->get_string()
                 << " of parameter " << form->nth(i)->get_name()->get_string()
                 << " does not conform to declared type " << form->nth(i)->get_type_decl()->get_string()
                 << ".\n";

            match = 0;
        }
    }

    return match;

}

// Semantic analysis for a conditional
int cond_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {
    int success = get_pred()->semant(classes, variables, currentClass);
    success = get_then_exp()->semant(classes, variables, currentClass) && success;
    success = get_else_exp()->semant(classes, variables, currentClass) && success;

    char* predType = get_pred()->get_type()->get_string();
    if(std::string(predType) != "Bool") {
        success = 0;
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Predicate of 'if' does not have type Bool.\n";
    }

    Class__class* common = classes.nearestCommonParent(get_then_exp()->get_type()->get_string(),
                                         get_else_exp()->get_type()->get_string());
    set_type(idtable.lookup_string(common->get_name()->get_string()));

    return success;
}

// Semantic analysis for a loop
int loop_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = get_pred()->semant(classes, variables, currentClass);
    success = get_body()->semant(classes, variables, currentClass) && success;

    char* predType = get_pred()->get_type()->get_string();
    if(std::string(predType) != "Bool") {
        success = 0;
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Loop condition does not have type Bool.\n";
    }

    set_type(get_body()->get_type());

    return success;
}

// Semantic analysis for a full case statement
int typcase_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = get_expr()->semant(classes, variables, currentClass);

    Class__class* commonParent = NULL;
    for(int i = get_cases()->first(); get_cases()->more(i); i = get_cases()->next(i)) {
        get_cases()->nth(i)->semant(classes, variables, currentClass);
        if(commonParent == NULL) {
            commonParent = classes.lookup(get_cases()->nth(i)->get_expr()->get_type()->get_string());
        } else {
            commonParent = classes.nearestCommonParent(commonParent->get_name()->get_string(),
                            get_cases()->nth(i)->get_expr()->get_type()->get_string());
        }
    }

    set_type(commonParent->get_name());

    return success;
}

// Semantic analysis for a block
int block_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = 1;

    Symbol inferredType = NULL;
    for(int i = get_sbody()->first(); get_sbody()->more(i); i = get_sbody()->next(i)) {
        success = get_sbody()->nth(i)->semant(classes, variables, currentClass) && success;
        if(!get_sbody()->more(i + 1)) {
            inferredType = get_sbody()->nth(i)->get_type();
        }
    }

    set_type(inferredType);

    return success;
}

// Semantic analysis for a let expression
int let_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {
    variables.enterscope();

    std::string varName = get_identifier()->get_string();
    std::string varType = get_type_decl()->get_string();

    if(varType == "SELF_TYPE") varType = currentClass->get_name()->get_string();

    int success = 1;
    if(classes.lookup(varType) == NULL) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Class " << varType << " of let-bound identifier "
             << varName << " is undefined.\n";
        success = 0;
    }

    if(varType == "SELF_TYPE") {
        char** currentClassName = new char*;
        *currentClassName = currentClass->get_name()->get_string();
        variables.addid(get_identifier()->get_string(), currentClassName);
    } else {
        char** typeName = new char*;
        *typeName = get_type_decl()->get_string();
        variables.addid(get_identifier()->get_string(), typeName);
    }

    idtable.add_string(get_identifier()->get_string());

    success = get_init()->semant(classes, variables, currentClass) && success;

    if(success && !classes.inheritsFrom(get_init()->get_type()->get_string(), varType)) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Inferred type " << get_init()->get_type()->get_string()
             << " of initialization of " << get_identifier()->get_string()
             << " does not conform to identifier's declared type "
             << get_type_decl()->get_string() << ".\n";

        success = 0;
    }

    success = get_body()->semant(classes, variables, currentClass) && success;

    set_type(get_body()->get_type());

    variables.exitscope();

    return success;
}

// Semantic analysis for addition
int plus_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);
    success = e2->semant(classes, variables, currentClass) && success;

    if(std::string(e1->get_type()->get_string()) != "Int" ||
       std::string(e2->get_type()->get_string()) != "Int") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": non-Int arguments: " << e1->get_type()->get_string()
             << " + " << e2->get_type()->get_string()
             << "\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Int"));

    return success;
}

// Semantic analysis for subtraction
int sub_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);
    success = e2->semant(classes, variables, currentClass) && success;

    if(std::string(e1->get_type()->get_string()) != "Int" ||
       std::string(e2->get_type()->get_string()) != "Int") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": non-Int arguments: " << e1->get_type()->get_string()
             << " - " << e2->get_type()->get_string()
             << "\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Int"));

    return success;
}

// Semantic analysis for multiplication
int mul_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);
    success = e2->semant(classes, variables, currentClass) && success;

    if(std::string(e1->get_type()->get_string()) != "Int" ||
       std::string(e2->get_type()->get_string()) != "Int") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": non-Int arguments: " << e1->get_type()->get_string()
             << " * " << e2->get_type()->get_string()
             << "\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Int"));

    return success;
}

// Semantic analysis for division
int divide_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);
    success = e2->semant(classes, variables, currentClass) && success;

    if(std::string(e1->get_type()->get_string()) != "Int" ||
       std::string(e2->get_type()->get_string()) != "Int") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": non-Int arguments: " << e1->get_type()->get_string()
             << " / " << e2->get_type()->get_string()
             << "\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Int"));

    return success;
}

// Semantic analysis for negation
int neg_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);

    if(std::string(e1->get_type()->get_string()) != "Int") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Argument of '~' has type "
             << e1->get_type()->get_string() << " instead of Int.\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Int"));

    return success;
}

// Semantic analysis for less-than comparison
int lt_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);
    success = e2->semant(classes, variables, currentClass) && success;

    if(std::string(e1->get_type()->get_string()) != "Int" ||
       std::string(e2->get_type()->get_string()) != "Int") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": non-Int arguments: " << e1->get_type()->get_string()
             << " < " << e2->get_type()->get_string()
             << "\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Bool"));

    return success;
}

// Semantic analysis for equality comparison
int eq_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);
    success = e2->semant(classes, variables, currentClass) && success;

    char* type1 = e1->get_type()->get_string();
    char* type2 = e2->get_type()->get_string();

    if((std::string(type1) == "Int" || std::string(type2) == "Int" ||
       std::string(type1) == "Bool" || std::string(type2) == "Bool" ||
       std::string(type1) == "String" || std::string(type2) == "String") &&
       std::string(type1) != std::string(type2)) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Illegal comparison with a basic type.\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Bool"));

    return success;
}

// Semantic analysis for less-than-or-equal comparison
int leq_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);
    success = e2->semant(classes, variables, currentClass) && success;

    if(std::string(e1->get_type()->get_string()) != "Int" ||
       std::string(e2->get_type()->get_string()) != "Int") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": non-Int arguments: " << e1->get_type()->get_string()
             << " <= " << e2->get_type()->get_string()
             << "\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Bool"));

    return success;
}

// Semantic analysis for logical complement
int comp_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);

    if(std::string(e1->get_type()->get_string()) != "Bool") {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Argument of 'not' has type "
             << e1->get_type()->get_string() << " instead of Bool.\n";
        success = 0;
    }

    set_type(idtable.lookup_string("Bool"));

    return success;
}

// Semantic analysis for integer constant
int int_const_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    set_type(idtable.lookup_string("Int"));

    return 1;
}

// Semantic analysis for boolean constant
int bool_const_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    set_type(idtable.lookup_string("Bool"));

    return 1;
}

// Semantic analysis for string constant
int string_const_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    set_type(idtable.lookup_string("String"));

    return 1;
}

// Semantic analysis for new keyword
int new__class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    std::string newType = type_name->get_string();
    if(newType == "SELF_TYPE") newType = currentClass->get_name()->get_string();

    Class__class* thisClass = classes.lookup(newType);

    if(thisClass == NULL) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": 'new' used with undefined class "
             << type_name->get_string() << ".\n";


        set_type(idtable.lookup_string("Object"));
        return 0;
    }

    set_type(idtable.lookup_string(thisClass->get_name()->get_string()));

    return 1;
}

// Semantic analysis for isvoid
int isvoid_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    int success = e1->semant(classes, variables, currentClass);

    set_type(idtable.lookup_string("Bool"));

    return success;
}

// Semantic analysis for empty expression
int no_expr_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    return 1;
}

// Semantic analysis for object
int object_class::semant(ClassTable& classes,
                       SymbolTable<std::string, char*>& variables, Class__class* currentClass) {

    char* thisClass = *variables.lookup(std::string(name->get_string()));

    if(thisClass == NULL) {
        classes.semant_error() << currentClass->get_filename() << ":" << get_line_number()
             << ": Undeclared identifier "
             << name->get_string() << ".\n";

        set_type(idtable.lookup_string("Object"));
        return 0;
    }

    set_type(idtable.lookup_string(thisClass));

    return 1;
}

/*   This is the entry point to the semantic checker.

     Your checker should do the following two things:

     1) Check that the program is semantically correct
     2) Decorate the abstract syntax tree with type information
        by setting the `type' field in each Expression node.
        (see `tree.h')

     You are free to first do 1), make sure you catch all semantic
     errors. Part 2) can be done in a second stage, when you want
     to build mycoolc.
 */
void program_class::semant()
{
    initialize_constants();

    /* ClassTable constructor may do some semantic analysis */
    ClassTable *classtable = new ClassTable(classes);

    if (classtable->errors()) {
    	cerr << "Compilation halted due to static semantic errors." << endl;
    	exit(1);
    } else {
        // We run the semantic analysis in each class
        classtable->semanticAnalysis();
        if (classtable->errors()) {
            cerr << "Compilation halted due to static semantic errors." << endl;
            exit(1);
        }
    }
}
