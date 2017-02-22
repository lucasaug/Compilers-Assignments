class C {
	a : Int;
	b : Bool;
	init(x : Int, y : Bool) : C {{
		b <- y;
		
		if x < a then
			a <- x
		else
			a <- a + 1
		fi;

		if 0 < x then
			let i : Int <- 0 in
				while i < x loop {
				    (new IO).out_int(i);
				    (new IO).out_string("\n");
					i <- i + 1;
				} pool
		else
			x
		fi;

		self;
    }};
};

class B inherits C {
    init(x : Int, y : Bool) : C {{
    	(new IO).out_string("init B\n");
    	new B;
    }};
};

Class Main inherits IO {
	main(): Object {{
	  (new C).init(10, true);
	  (new B).init(10, true);
	  (new B)@C.init(10, true);

	  out_int(1 + 2);
	  out_string("\ntest\n");

	  case 1 + 4 of
	      test1 : Int => test1;
	      test2 : String => test2;
	  esac;
	}};
};
