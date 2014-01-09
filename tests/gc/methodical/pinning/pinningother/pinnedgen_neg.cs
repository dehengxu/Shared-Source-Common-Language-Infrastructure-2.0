// ==++==
//
//   
//    Copyright (c) 2006 Microsoft Corporation.  All rights reserved.
//   
//    The use and distribution terms for this software are contained in the file
//    named license.txt, which can be found in the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by the
//    terms of this license.
//   
//    You must not remove this notice, or any other, from this software.
//   
//
// ==--==

// Tests Pinning and Generations of objects pinned
// Pinning doesn't just pin an instance.  It pins all contiguous instances that have 
// no intervening garbage. This is an intentional effect of our design & implementation.
// So in the test below, even if we pin only arr1, both arrays wont be collected before 
// the first Collect call.
// This test checks what happens when there is intervening garbage in contiguous instances.


using System;
using System.Runtime.InteropServices;

public class Test {

	public static void Main() {
		
		int[] arr1 = new int[100];
		int[] arr2= new int[100];
		int[] arr3 = new int[100];
		int[] arr4= new int[100];
		bool collected=false;

		GCHandle handle = GCHandle.Alloc(arr1,GCHandleType.Pinned);	// only pin one
	
		int gen11,gen21,gen31, gen41;
		gen11 = GC.GetGeneration(arr1);
		gen21 = GC.GetGeneration(arr2);
		gen31 = GC.GetGeneration(arr3);
		gen41 = GC.GetGeneration(arr4);

		Console.WriteLine("Gen arr1: {0}",gen11);
		Console.WriteLine("Gen arr2: {0}",gen21);
		Console.WriteLine("Gen arr3: {0}",gen31);
		Console.WriteLine("Gen arr4: {0}",gen41);

		Console.WriteLine("Deleting arr3 and arr4");
		arr3=null;
		arr4=null;

		GC.Collect();
		GC.WaitForPendingFinalizers();

		int gen12, gen22;
		gen12 = GC.GetGeneration(arr1);
		gen22 = GC.GetGeneration(arr2);

		Console.WriteLine("Gen arr1: {0}",gen12);
		Console.WriteLine("Gen arr2: {0}",gen22);
		
		try {
			Console.WriteLine("Gen arr3: {0}", GC.GetGeneration(arr3));
			Console.WriteLine("Gen arr4: {0}", GC.GetGeneration(arr4));
		}catch(Exception e) {
			Console.WriteLine("Exception thrown because arr3,arr4 are collected");
			Console.WriteLine("Caught: {0}",e);
			collected=true;
		}

		if((gen11==gen21) && (gen12==gen22) && collected==true) {
			Console.WriteLine("Test Passed");
			Environment.ExitCode=0;
		} else {
			Console.WriteLine("Test Failed");
			Environment.ExitCode=1;
		}
			
	}
}
