 *******************
 Author: Chu-An Tsai
 CSCI 503 Operating Systems
 Part 1 - MPMC
 *******************
 1. To compile: make

 2. To launch: ./mpmc Number_Producers Number_Consumers Max_Sleep_Seconds Total_Number_Items2Produce Ring_Buffer_Size
 	
 3. I track the number of the items produced and consumed, so I can use a if function 
    at the end to compare if #TotalProduced = #TotalConsumed = Total_number_item2Produce. 
