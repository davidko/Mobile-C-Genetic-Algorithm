#include <string>

class Gene
{
  public:
    Gene(int size=120);
    Gene(double* genes, size_t size);
    Gene(int* genes, size_t size);
    ~Gene();
    std::string str();

  private:
    double *_gene;
    size_t _size;
};
