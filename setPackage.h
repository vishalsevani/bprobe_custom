typedef enum {BASE, DERIVED, UNKNOWN} setClass ;

typedef struct setID {
  unsigned int setID;
  setClass class;
} SetID;

typedef struct setIDList {
  SetID setID;
  struct setIDList *next;
} SetIDList, *SetIDListPtr;

typedef struct element {
  float item;
  float itemLow;
  float itemHigh;
  int elementCount;
  int savedElementCount;
  int setCount;
  SetIDListPtr setList;
  SetID setID;
  struct element *next;
} Element;

typedef Element *ElementPtr;

typedef struct {
  int count;
  Element *first;
} Set;

typedef Set *SetPtr;


Set *createSet(float [], int count, float fuzzFactor);

void adjustFuzz(Set *set, float fuzzFactor);

void clearFuzz(Set *set);

int setSize(Set *set);

Set *intersectSets(Set *setA, Set * setB);

Set *unionSets(Set *setA, Set * setB);

void dumpSet(Set *set, int checkZero);

void dumpElement(Element *element, int checkZero);

Set *destroySet(Set *set);

void addSetIDToList(Element *element, SetID setId);

void addSetsToIDList(Element *destElement, Element *sourceElement);

void dumpSetIDList(Element *element);

Element *destroyList(Element *);

Element * destroyElement(Element *element);

Element * makeElement();

Element * copyElement(Element *);

SetIDListPtr copyIDList(SetIDListPtr oldList);

void findPeakInSet(Set *inSet, Set **outSet, int *peakSize, int *numSets);

Set *makeSet();

#define UNION 1
#define INTERSECTION 2



