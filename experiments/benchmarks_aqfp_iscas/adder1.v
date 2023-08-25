module top( a , b , c , cout , s );
  input a , b , c ;
  output cout , s ;
  wire n4 , n5 , n6 , n7 , n8 , n9 , n10 ;
  assign n4 = a & b ;
  assign n5 = a | b ;
  assign n6 = ~n4 & n5 ;
  assign n7 = c & n6 ;
  assign n8 = n4 | n7 ;
  assign n9 = c | n6 ;
  assign n10 = ~n7 & n9 ;
  assign cout = n8 ;
  assign s = n10 ;
endmodule
