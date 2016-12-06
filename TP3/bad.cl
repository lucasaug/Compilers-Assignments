(* no error *)
class A {
};

(* error:  b is not a type identifier *)
Class b inherits A {
};

(* error:  a is not a type identifier *)
Class C inherits a {
};

(* error:  keyword inherits is misspelled *)
Class D inherts A {
};

(* error:  closing brace is missing *)
Class E inherits A {
;

(* Entirely wrong class definition *)
Class Mistake {
	if 1 = 1 then "1" else "0";
};

Class X {
	(* Invalid feature *)
	if 1 = 1 then "1" else "0";

	(* Invalid attribute *)
	test :- Int;

	(* Invalid method *)
    test2() : Int inherits E {
        if false then
            0
        else
            1
        fi
            
    };

    test3() : Int {{
		(* keyword class inside let definition *)
		let class, a : Int <- 0 in {
			a + 2
		};

		(* let definition entirely wrong *)
		let wrong in {
			wrong
		};
	}};
};
