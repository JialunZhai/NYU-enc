#!/bin/bash

NYUENC_PATH=nyuenc-grading

rm -rf $NYUENC_PATH
mkdir $NYUENC_PATH
tar xf nyuenc*.tar.xz -C $NYUENC_PATH

mkdir myoutputs


MAKEFILE="$NYUENC_PATH/Makefile"
if [[ ! -f "$MAKEFILE" ]]
	then 
		echo "There must be a Makefile in the archive. Aborting."
		exit 1
fi


NYUENC_C="$NYUENC_PATH/nyuenc.c"
NYUENC_CPP="$NYUENC_PATH/nyuenc.cpp"

if [ ! -f "$NYUENC_C" ] && [ ! -f "$NYUENC_CPP" ]
	then 
		echo "The driver function of the code (or the entire code) must be in a file called 'nyuenc.c' or 'nyuenc.cpp'. Please check the name and try again."
		exit 1
fi

module load gcc-9.2
make -C $NYUENC_PATH

NYUENC="$NYUENC_PATH/nyuenc"

if [[ ! -f "$NYUENC" ]]
	then 
		echo "There was an error compiling the file 'nyuenc.c'. Please contact a grader if the file 'nyuenc.c' compiles correctly when done manually."
		exit 1
fi

run_test() {
  echo -n "Test $1: "
  
  $NYUENC ${@:2} inputs/$1.in > myoutputs/$1.myout
  
  if [[ ! -f "refoutputs/$1.out" ]]
    then 
      echo "Reference output is missing for Test Case $1. Please check and try again."
      exit 1
  fi

  if cmp -s -b refoutputs/$1.out myoutputs/$1.myout;
    then	
      echo "Passed"
  else
    echo "~~Failed~~"
    echo "Expected output                                                     |   Your output"
    echo "========================================================================================================================================"
    diff -y -W138 <(xxd refoutputs/$1.out) <(xxd myoutputs/$1.myout) | head
  fi
  
}

run_test_multifile() {
  echo -n "Test $1: "
  
  $NYUENC ${@:2} $(for i in {1..10}; do echo -n "inputs/$1.in "; done) > myoutputs/$1.myout

  if [[ ! -f "refoutputs/$1.out" ]]
    then 
      echo "Reference output is missing for Test Case $1. Please check and try again."
      exit 1
  fi

  if cmp -s -b refoutputs/$1.out myoutputs/$1.myout;
    then	
      echo "Passed"
  else
    echo "~~Failed~~"
    echo "Expected output                                                     |   Your output"
    echo "========================================================================================================================================"
    diff -y -W138 <(xxd refoutputs/$1.out) <(xxd myoutputs/$1.myout) | head
  fi
}

# Milestone 1
run_test 1
run_test 2
run_test 3
run_test_multifile 4

# Milestone 2
run_test 5 -j 3
run_test_multifile 6 -j 3

#Cleanup
rm -rf $NYUENC_PATH
