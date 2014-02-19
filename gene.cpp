#include "gene.h"
#include <sstream>

Gene::Gene(int size)
{
  _gene = new double[size];
  _size = size;
}

Gene::Gene(double *genes, size_t size)
{
  _gene = new double[size];
  int i;
  for(i = 0; i < size; i++) {
    _gene[i] = genes[i];
  }
  _size = size;
}

Gene::Gene(int* genes, size_t size)
{
  _gene = new double[size];
  int i;
  for(i = 0; i < size; i++) {
    _gene[i] = genes[i];
  }
  _size = size;
}

Gene::~Gene()
{
  delete[] _gene;
}

std::string Gene::str()
{
  std::ostringstream stream;
  int i;
  for(i = 0; i < _size; i++) {
    stream << _gene[i] << "\n";
  }
  return stream.str();
}
