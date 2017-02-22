class C inherits B {
	a : Int;
	b : Bool;
	init(x : Int, y : Bool) : C {
           {
        (* invalid assignment *)
		a <- y;
		b <- x;
		self;
           }
	};
};

(* The part below can be uncommented to trigger
an error
We don't do this to test for other errors *)
class B (*inherits C*) {};

Class Main {
	main():C {
	 {
      (* invalid types for addition *)
	  1 + "test";
	  (* invalid argument types *)
	  (new C).init(1,1);
	  (* wrong number of arguments *)
	  (new C).init(1,true,3);
	  (* method iinit does not exist *)
	  (new C).iinit(1,true);

	  (* Invalid comparison *)
	  (new IO) < (new Int);

	  (* wrong return type *)

	 }
	};
};
