#ifndef HOME_CONTROLLER_H
#define HOME_CONTROLLER_H

#include "../../framework/http.h"

void HomeController_index(Request *req, Response *res);
void HomeController_about(Request *req, Response *res);
void HomeController_docs(Request *req, Response *res);
void HomeController_reactive(Request *req, Response *res);

#endif /* HOME_CONTROLLER_H */
