module top( x0 , x1 , x2 , x3 , x4 , x5 , x6 , x7 , x8 , y0 );
  input x0 , x1 , x2 , x3 , x4 , x5 , x6 , x7 , x8 ;
  output y0 ;
  wire n10 , n11 , n12 , n13 , n14 , n15 , n16 , n17 , n18 , n19 , n20 , n21 , n22 , n23 , n24 , n25 , n26 , n27 , n28 , n29 , n30 , n31 , n32 , n33 , n34 , n35 , n36 , n37 , n38 , n39 , n40 , n41 , n42 , n43 , n44 , n45 , n46 , n47 , n48 , n49 , n50 , n51 , n52 , n53 , n54 , n55 , n56 , n57 , n58 , n59 , n60 , n61 , n62 , n63 , n64 , n65 , n66 , n67 , n68 , n69 , n70 , n71 , n72 , n73 , n74 , n75 , n76 , n77 , n78 , n79 , n80 , n81 , n82 , n83 , n84 , n85 , n86 , n87 , n88 , n89 , n90 , n91 , n92 , n93 , n94 , n95 , n96 , n97 , n98 , n99 , n100 , n101 , n102 , n103 , n104 , n105 , n106 , n107 , n108 , n109 , n110 , n111 , n112 , n113 , n114 , n115 , n116 , n117 , n118 , n119 , n120 , n121 , n122 , n123 , n124 , n125 , n126 , n127 , n128 , n129 , n130 , n131 , n132 , n133 , n134 , n135 , n136 , n137 , n138 , n139 , n140 , n141 , n142 , n143 , n144 , n145 , n146 , n147 , n148 , n149 , n150 , n151 , n152 , n153 , n154 , n155 , n156 , n157 , n158 , n159 , n160 , n161 , n162 , n163 , n164 , n165 , n166 , n167 , n168 , n169 , n170 , n171 , n172 , n173 , n174 , n175 , n176 , n177 , n178 , n179 , n180 , n181 , n182 , n183 , n184 , n185 , n186 , n187 , n188 , n189 , n190 , n191 , n192 , n193 , n194 , n195 , n196 , n197 , n198 , n199 , n200 , n201 , n202 , n203 , n204 , n205 , n206 , n207 , n208 , n209 ;
  assign n10 = ( x0 & ~x1 ) | ( x0 & x7 ) | ( ~x1 & x7 ) ;
  assign n11 = ( x0 & ~x2 ) | ( x0 & n10 ) | ( ~x2 & n10 ) ;
  assign n12 = x0 & ~n11 ;
  assign n13 = n11 | n12 ;
  assign n14 = ( ~x0 & n12 ) | ( ~x0 & n13 ) | ( n12 & n13 ) ;
  assign n15 = ( x3 & x4 ) | ( x3 & x6 ) | ( x4 & x6 ) ;
  assign n16 = ( ~x5 & x6 ) | ( ~x5 & n15 ) | ( x6 & n15 ) ;
  assign n17 = x6 & ~n16 ;
  assign n18 = n16 | n17 ;
  assign n19 = ( ~x6 & n17 ) | ( ~x6 & n18 ) | ( n17 & n18 ) ;
  assign n20 = n14 & ~n19 ;
  assign n21 = x0 & ~x1 ;
  assign n22 = x0 | x1 ;
  assign n23 = ( ~x0 & n21 ) | ( ~x0 & n22 ) | ( n21 & n22 ) ;
  assign n24 = ~x5 & x7 ;
  assign n25 = ~x6 & n24 ;
  assign n26 = x2 & ~x4 ;
  assign n27 = ( x3 & n25 ) | ( x3 & n26 ) | ( n25 & n26 ) ;
  assign n28 = ~x3 & n27 ;
  assign n29 = x5 & ~x7 ;
  assign n30 = x6 & n29 ;
  assign n31 = x4 & n30 ;
  assign n32 = ( x2 & x3 ) | ( x2 & n31 ) | ( x3 & n31 ) ;
  assign n33 = ~x2 & n32 ;
  assign n34 = n23 & n33 ;
  assign n35 = ( n23 & n28 ) | ( n23 & n34 ) | ( n28 & n34 ) ;
  assign n36 = x1 & ~x3 ;
  assign n37 = ( x0 & x2 ) | ( x0 & n36 ) | ( x2 & n36 ) ;
  assign n38 = ~x2 & n37 ;
  assign n39 = ~x1 & x3 ;
  assign n40 = ( x0 & x2 ) | ( x0 & n39 ) | ( x2 & n39 ) ;
  assign n41 = ~x0 & n40 ;
  assign n42 = x4 & x5 ;
  assign n43 = ( x6 & x7 ) | ( x6 & n42 ) | ( x7 & n42 ) ;
  assign n44 = ~x7 & n43 ;
  assign n45 = n41 & n44 ;
  assign n46 = ( x4 & ~x6 ) | ( x4 & n24 ) | ( ~x6 & n24 ) ;
  assign n47 = ~x4 & n46 ;
  assign n48 = n45 | n47 ;
  assign n49 = ( n38 & n45 ) | ( n38 & n48 ) | ( n45 & n48 ) ;
  assign n50 = n35 | n49 ;
  assign n51 = ( n14 & ~n20 ) | ( n14 & n50 ) | ( ~n20 & n50 ) ;
  assign n52 = x3 & x4 ;
  assign n53 = x0 & ~x2 ;
  assign n54 = x1 & n53 ;
  assign n55 = n30 & n54 ;
  assign n56 = ~x0 & x2 ;
  assign n57 = ~x1 & n56 ;
  assign n58 = n55 | n57 ;
  assign n59 = ( n25 & n55 ) | ( n25 & n58 ) | ( n55 & n58 ) ;
  assign n60 = ( x2 & ~x5 ) | ( x2 & x7 ) | ( ~x5 & x7 ) ;
  assign n61 = ( x2 & ~x6 ) | ( x2 & n60 ) | ( ~x6 & n60 ) ;
  assign n62 = x2 & ~n61 ;
  assign n63 = n61 | n62 ;
  assign n64 = ( ~x2 & n62 ) | ( ~x2 & n63 ) | ( n62 & n63 ) ;
  assign n65 = n59 | n64 ;
  assign n66 = ( n23 & n59 ) | ( n23 & n65 ) | ( n59 & n65 ) ;
  assign n67 = ( x3 & x4 ) | ( x3 & n66 ) | ( x4 & n66 ) ;
  assign n68 = ( n51 & ~n52 ) | ( n51 & n67 ) | ( ~n52 & n67 ) ;
  assign n69 = x5 & x6 ;
  assign n70 = x3 & n54 ;
  assign n71 = ( x4 & x7 ) | ( x4 & n70 ) | ( x7 & n70 ) ;
  assign n72 = ~x7 & n71 ;
  assign n73 = ( x3 & x4 ) | ( x3 & ~n14 ) | ( x4 & ~n14 ) ;
  assign n74 = x3 & ~n73 ;
  assign n75 = ( x4 & ~n73 ) | ( x4 & n74 ) | ( ~n73 & n74 ) ;
  assign n76 = ~x4 & x7 ;
  assign n77 = ( x3 & n57 ) | ( x3 & n76 ) | ( n57 & n76 ) ;
  assign n78 = ~x3 & n77 ;
  assign n79 = n75 | n78 ;
  assign n80 = n72 | n79 ;
  assign n81 = ( x2 & ~x3 ) | ( x2 & x7 ) | ( ~x3 & x7 ) ;
  assign n82 = ( x2 & ~x4 ) | ( x2 & n81 ) | ( ~x4 & n81 ) ;
  assign n83 = x2 & ~n82 ;
  assign n84 = n82 | n83 ;
  assign n85 = ( ~x2 & n83 ) | ( ~x2 & n84 ) | ( n83 & n84 ) ;
  assign n86 = n80 | n85 ;
  assign n87 = ( n23 & n80 ) | ( n23 & n86 ) | ( n80 & n86 ) ;
  assign n88 = ( x5 & x6 ) | ( x5 & n87 ) | ( x6 & n87 ) ;
  assign n89 = ( n68 & ~n69 ) | ( n68 & n88 ) | ( ~n69 & n88 ) ;
  assign n90 = ~x4 & x5 ;
  assign n91 = x2 & ~x3 ;
  assign n92 = x2 | x3 ;
  assign n93 = ( ~x2 & n91 ) | ( ~x2 & n92 ) | ( n91 & n92 ) ;
  assign n94 = ( x1 & x6 ) | ( x1 & n93 ) | ( x6 & n93 ) ;
  assign n95 = ( x0 & ~x1 ) | ( x0 & n94 ) | ( ~x1 & n94 ) ;
  assign n96 = ( x0 & x6 ) | ( x0 & ~n94 ) | ( x6 & ~n94 ) ;
  assign n97 = n95 & ~n96 ;
  assign n98 = ( x3 & x6 ) | ( x3 & n23 ) | ( x6 & n23 ) ;
  assign n99 = ( x2 & ~x3 ) | ( x2 & n98 ) | ( ~x3 & n98 ) ;
  assign n100 = ( x2 & x6 ) | ( x2 & ~n98 ) | ( x6 & ~n98 ) ;
  assign n101 = n99 & ~n100 ;
  assign n102 = n97 | n101 ;
  assign n103 = ( ~x4 & x5 ) | ( ~x4 & n102 ) | ( x5 & n102 ) ;
  assign n104 = ( n90 & n102 ) | ( n90 & ~n103 ) | ( n102 & ~n103 ) ;
  assign n105 = x4 & x6 ;
  assign n106 = ( x3 & x5 ) | ( x3 & n105 ) | ( x5 & n105 ) ;
  assign n107 = ~x3 & n106 ;
  assign n108 = ~x1 & n107 ;
  assign n109 = ( x0 & ~x2 ) | ( x0 & n108 ) | ( ~x2 & n108 ) ;
  assign n110 = ~x0 & n109 ;
  assign n111 = ( x0 & x1 ) | ( x0 & x2 ) | ( x1 & x2 ) ;
  assign n112 = ( x0 & x1 ) | ( x0 & ~n111 ) | ( x1 & ~n111 ) ;
  assign n113 = ( x2 & x3 ) | ( x2 & n112 ) | ( x3 & n112 ) ;
  assign n114 = n111 & ~n113 ;
  assign n115 = ( ~n111 & n113 ) | ( ~n111 & n114 ) | ( n113 & n114 ) ;
  assign n116 = n114 | n115 ;
  assign n117 = ( x5 & x6 ) | ( x5 & n116 ) | ( x6 & n116 ) ;
  assign n118 = ( x4 & ~x5 ) | ( x4 & n117 ) | ( ~x5 & n117 ) ;
  assign n119 = ( x4 & x6 ) | ( x4 & ~n117 ) | ( x6 & ~n117 ) ;
  assign n120 = n118 & ~n119 ;
  assign n121 = x3 & ~x5 ;
  assign n122 = ( x4 & ~x6 ) | ( x4 & n121 ) | ( ~x6 & n121 ) ;
  assign n123 = ~x4 & n122 ;
  assign n124 = x0 & x2 ;
  assign n125 = ( x1 & ~n123 ) | ( x1 & n124 ) | ( ~n123 & n124 ) ;
  assign n126 = n123 & n125 ;
  assign n127 = n120 | n126 ;
  assign n128 = ( ~n104 & n110 ) | ( ~n104 & n127 ) | ( n110 & n127 ) ;
  assign n129 = n104 | n128 ;
  assign n130 = ~x0 & x1 ;
  assign n131 = x1 & ~n130 ;
  assign n132 = x2 & ~x8 ;
  assign n133 = x3 & n132 ;
  assign n134 = n131 & n133 ;
  assign n135 = ~x2 & x8 ;
  assign n136 = ~x3 & n135 ;
  assign n137 = ( ~n130 & n131 ) | ( ~n130 & n136 ) | ( n131 & n136 ) ;
  assign n138 = ( ~x0 & n134 ) | ( ~x0 & n137 ) | ( n134 & n137 ) ;
  assign n139 = ( x4 & x5 ) | ( x4 & x7 ) | ( x5 & x7 ) ;
  assign n140 = ( ~x6 & x7 ) | ( ~x6 & n139 ) | ( x7 & n139 ) ;
  assign n141 = x7 & ~n140 ;
  assign n142 = n140 | n141 ;
  assign n143 = ( ~x7 & n141 ) | ( ~x7 & n142 ) | ( n141 & n142 ) ;
  assign n144 = n138 & ~n143 ;
  assign n145 = x6 & ~x8 ;
  assign n146 = x7 & n145 ;
  assign n147 = x5 & ~n90 ;
  assign n148 = n146 & n147 ;
  assign n149 = ~x6 & x8 ;
  assign n150 = ~x7 & n149 ;
  assign n151 = ( ~n90 & n147 ) | ( ~n90 & n150 ) | ( n147 & n150 ) ;
  assign n152 = ( ~x4 & n148 ) | ( ~x4 & n151 ) | ( n148 & n151 ) ;
  assign n153 = n23 & n152 ;
  assign n154 = n93 & n153 ;
  assign n155 = n38 & n152 ;
  assign n156 = ( n41 & n152 ) | ( n41 & n155 ) | ( n152 & n155 ) ;
  assign n157 = n154 | n156 ;
  assign n158 = ( n138 & ~n144 ) | ( n138 & n157 ) | ( ~n144 & n157 ) ;
  assign n159 = ~x2 & x3 ;
  assign n160 = x3 & ~n159 ;
  assign n161 = n146 & n160 ;
  assign n162 = ( n150 & ~n159 ) | ( n150 & n160 ) | ( ~n159 & n160 ) ;
  assign n163 = ( ~x2 & n161 ) | ( ~x2 & n162 ) | ( n161 & n162 ) ;
  assign n164 = n131 & n146 ;
  assign n165 = ( ~n130 & n131 ) | ( ~n130 & n150 ) | ( n131 & n150 ) ;
  assign n166 = ( ~x0 & n164 ) | ( ~x0 & n165 ) | ( n164 & n165 ) ;
  assign n167 = n93 & n166 ;
  assign n168 = n23 | n167 ;
  assign n169 = ( n163 & n167 ) | ( n163 & n168 ) | ( n167 & n168 ) ;
  assign n170 = ( x4 & x5 ) | ( x4 & n169 ) | ( x5 & n169 ) ;
  assign n171 = ( ~n42 & n158 ) | ( ~n42 & n170 ) | ( n158 & n170 ) ;
  assign n172 = x6 & x7 ;
  assign n173 = x4 & ~x8 ;
  assign n174 = x5 & n173 ;
  assign n175 = n160 & n174 ;
  assign n176 = ~x4 & x8 ;
  assign n177 = ~x5 & n176 ;
  assign n178 = ( ~n159 & n160 ) | ( ~n159 & n177 ) | ( n160 & n177 ) ;
  assign n179 = ( ~x2 & n175 ) | ( ~x2 & n178 ) | ( n175 & n178 ) ;
  assign n180 = n131 & n174 ;
  assign n181 = ( ~n130 & n131 ) | ( ~n130 & n177 ) | ( n131 & n177 ) ;
  assign n182 = ( ~x0 & n180 ) | ( ~x0 & n181 ) | ( n180 & n181 ) ;
  assign n183 = ( x4 & x5 ) | ( x4 & ~n138 ) | ( x5 & ~n138 ) ;
  assign n184 = x4 & ~n183 ;
  assign n185 = ( x5 & ~n183 ) | ( x5 & n184 ) | ( ~n183 & n184 ) ;
  assign n186 = n93 | n185 ;
  assign n187 = ( n182 & n185 ) | ( n182 & n186 ) | ( n185 & n186 ) ;
  assign n188 = n23 | n187 ;
  assign n189 = ( n179 & n187 ) | ( n179 & n188 ) | ( n187 & n188 ) ;
  assign n190 = ( x6 & x7 ) | ( x6 & n189 ) | ( x7 & n189 ) ;
  assign n191 = ( n171 & ~n172 ) | ( n171 & n190 ) | ( ~n172 & n190 ) ;
  assign n192 = ( x0 & x1 ) | ( x0 & x5 ) | ( x1 & x5 ) ;
  assign n193 = ( ~x4 & x5 ) | ( ~x4 & n192 ) | ( x5 & n192 ) ;
  assign n194 = x5 & ~n193 ;
  assign n195 = n193 | n194 ;
  assign n196 = ( ~x5 & n194 ) | ( ~x5 & n195 ) | ( n194 & n195 ) ;
  assign n197 = n93 & n196 ;
  assign n198 = ( x2 & x3 ) | ( x2 & x5 ) | ( x3 & x5 ) ;
  assign n199 = ( ~x4 & x5 ) | ( ~x4 & n198 ) | ( x5 & n198 ) ;
  assign n200 = x5 & ~n199 ;
  assign n201 = n199 | n200 ;
  assign n202 = ( ~x5 & n200 ) | ( ~x5 & n201 ) | ( n200 & n201 ) ;
  assign n203 = n197 | n202 ;
  assign n204 = ( n23 & n197 ) | ( n23 & n203 ) | ( n197 & n203 ) ;
  assign n205 = ( x4 & x5 ) | ( x4 & n116 ) | ( x5 & n116 ) ;
  assign n206 = ( ~n42 & n204 ) | ( ~n42 & n205 ) | ( n204 & n205 ) ;
  assign n207 = n191 | n206 ;
  assign n208 = ( ~n89 & n129 ) | ( ~n89 & n207 ) | ( n129 & n207 ) ;
  assign n209 = n89 | n208 ;
  assign y0 = n209 ;
endmodule
