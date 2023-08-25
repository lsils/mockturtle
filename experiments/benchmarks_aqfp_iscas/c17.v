module top( N1 , N2 , N3 , N6 , N7 , N22 , N23 );
  input N1 , N2 , N3 , N6 , N7 ;
  output N22 , N23 ;
  wire n6 , n7 , n8 , n9 , n10 , n11 ;
  assign n6 = N1 & N3 ;
  assign n7 = N3 & N6 ;
  assign n8 = N2 & ~n7 ;
  assign n9 = n6 | n8 ;
  assign n10 = N2 | N7 ;
  assign n11 = ~n7 & n10 ;
  assign N22 = n9 ;
  assign N23 = n11 ;
endmodule
